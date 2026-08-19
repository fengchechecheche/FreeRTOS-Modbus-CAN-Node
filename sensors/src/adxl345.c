#include "adxl345.h"

#include <limits.h>
#include <string.h>

#define ADXL345_MAX_RECOVERY_ATTEMPTS (1U)
#define ADXL345_DATA_FORMAT_VERIFY_MASK UINT8_C(0x7f)
#define ADXL345_BW_RATE_VERIFY_MASK UINT8_C(0x1f)

static uint32_t adxl345_saturating_increment(uint32_t value)
{
  return value == UINT32_MAX ? UINT32_MAX : value + 1U;
}

static uint32_t adxl345_saturating_add(uint32_t value, uint32_t increment)
{
  return increment > (UINT32_MAX - value) ? UINT32_MAX : value + increment;
}

static bool adxl345_time_reached(uint32_t now_ms, uint32_t target_ms)
{
  return (int32_t)(now_ms - target_ms) >= 0;
}

static int16_t adxl345_decode_s16(const uint8_t *data)
{
  const uint16_t raw =
      (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
  return raw <= (uint16_t)INT16_MAX
             ? (int16_t)raw
             : (int16_t)((int32_t)raw - INT32_C(65536));
}

static uint64_t adxl345_integer_root(uint64_t value)
{
  uint64_t result = 0U;
  uint64_t bit = UINT64_C(1) << 62U;
  while (bit > value)
  {
    bit >>= 2U;
  }
  while (bit != 0U)
  {
    if (value >= (result + bit))
    {
      value -= result + bit;
      result = (result >> 1U) + bit;
    }
    else
    {
      result >>= 1U;
    }
    bit >>= 2U;
  }
  return result;
}

static uint64_t adxl345_square_signed(int64_t value)
{
  const uint64_t magnitude =
      value < 0 ? (uint64_t)(-value) : (uint64_t)value;
  return magnitude * magnitude;
}

bool adxl345_parse_xyz(const uint8_t data[ADXL345_DATA_LENGTH],
                       int16_t raw_xyz[ADXL345_AXIS_COUNT])
{
  if ((data == NULL) || (raw_xyz == NULL))
  {
    return false;
  }
  for (size_t axis = 0U; axis < ADXL345_AXIS_COUNT; ++axis)
  {
    raw_xyz[axis] = adxl345_decode_s16(&data[axis * 2U]);
  }
  return true;
}

int32_t adxl345_counts_to_millig(int32_t corrected_counts)
{
  const int64_t scaled = (int64_t)corrected_counts * INT64_C(1000);
  return scaled >= 0 ? (int32_t)((scaled + INT64_C(128)) / INT64_C(256))
                     : (int32_t)((scaled - INT64_C(128)) / INT64_C(256));
}

void adxl345_feature_window_initialize(adxl345_feature_window_t *window)
{
  if (window != NULL)
  {
    (void)memset(window, 0, sizeof(*window));
  }
}

bool adxl345_feature_window_push(adxl345_feature_window_t *window,
                                 const adxl345_sample_t *sample,
                                 adxl345_feature_t *feature)
{
  if ((window == NULL) || (sample == NULL) || (feature == NULL) ||
      ((sample->quality_flags & ADXL345_QUALITY_VALID) == 0U))
  {
    return false;
  }

  for (size_t axis = 0U; axis < ADXL345_AXIS_COUNT; ++axis)
  {
    const int32_t value = sample->acceleration_millig[axis];
    if (window->count == 0U)
    {
      window->minimum_millig[axis] = value;
      window->maximum_millig[axis] = value;
    }
    else
    {
      if (value < window->minimum_millig[axis])
      {
        window->minimum_millig[axis] = value;
      }
      if (value > window->maximum_millig[axis])
      {
        window->maximum_millig[axis] = value;
      }
    }
    window->sum_millig[axis] += value;
    window->sum_square_millig[axis] +=
        adxl345_square_signed((int64_t)value);
  }
  ++window->count;
  window->quality_flags |=
      sample->quality_flags & ~ADXL345_QUALITY_VALID;
  window->dropped_sample_lower_bound = adxl345_saturating_add(
      window->dropped_sample_lower_bound,
      sample->dropped_sample_lower_bound);

  if (window->count < ADXL345_FEATURE_WINDOW_SAMPLES)
  {
    return false;
  }

  uint64_t resultant_square = 0U;
  for (size_t axis = 0U; axis < ADXL345_AXIS_COUNT; ++axis)
  {
    const int64_t sum = window->sum_millig[axis];
    const uint64_t variance_numerator =
        (uint64_t)ADXL345_FEATURE_WINDOW_SAMPLES *
            window->sum_square_millig[axis] -
        adxl345_square_signed(sum);
    const uint64_t rms =
        adxl345_integer_root(variance_numerator) /
        ADXL345_FEATURE_WINDOW_SAMPLES;
    const int32_t mean =
        (int32_t)(sum / (int64_t)ADXL345_FEATURE_WINDOW_SAMPLES);
    const int64_t minimum_delta =
        (int64_t)mean - window->minimum_millig[axis];
    const int64_t maximum_delta =
        (int64_t)window->maximum_millig[axis] - mean;
    const uint64_t peak =
        (uint64_t)(minimum_delta > maximum_delta ? minimum_delta
                                                 : maximum_delta);

    feature->mean_millig[axis] = mean;
    feature->rms_millig[axis] = (uint32_t)rms;
    feature->peak_abs_millig[axis] = (uint32_t)peak;
    resultant_square += rms * rms;
  }
  feature->sequence = adxl345_saturating_increment(feature->sequence);
  feature->sample_count = ADXL345_FEATURE_WINDOW_SAMPLES;
  feature->resultant_rms_millig =
      (uint32_t)adxl345_integer_root(resultant_square);
  feature->quality_flags = ADXL345_QUALITY_VALID | window->quality_flags;
  feature->dropped_sample_lower_bound =
      window->dropped_sample_lower_bound;
  adxl345_feature_window_initialize(window);
  return true;
}

static adxl345_status_t adxl345_map_transport(
    adxl345_transport_result_t result)
{
  switch (result)
  {
    case ADXL345_TRANSPORT_INVALID_ARGUMENT:
      return ADXL345_STATUS_TRANSPORT_INVALID_ARGUMENT;
    case ADXL345_TRANSPORT_BUSY:
      return ADXL345_STATUS_TRANSPORT_BUSY;
    case ADXL345_TRANSPORT_TIMEOUT:
      return ADXL345_STATUS_TRANSPORT_TIMEOUT;
    case ADXL345_TRANSPORT_IO_ERROR:
    default:
      return ADXL345_STATUS_TRANSPORT_IO_ERROR;
  }
}

static uint32_t adxl345_error_quality(adxl345_status_t status)
{
  if (status == ADXL345_STATUS_CONFIGURATION_MISMATCH)
  {
    return ADXL345_QUALITY_CONFIGURATION_ERROR;
  }
  if (status == ADXL345_STATUS_DATA_READY_STALLED)
  {
    return ADXL345_QUALITY_STALLED;
  }
  return ADXL345_QUALITY_TRANSPORT_ERROR;
}

static void adxl345_mark_error(adxl345_t *driver,
                               adxl345_status_t status)
{
  driver->last_error_status = status;
  driver->status = status;
  driver->sample.status = status;
  driver->sample.quality_flags &= ~ADXL345_QUALITY_VALID;
  driver->sample.quality_flags |= adxl345_error_quality(status);
  driver->error_count = adxl345_saturating_increment(driver->error_count);
  driver->pending_gap = true;
}

static void adxl345_request_recovery_internal(adxl345_t *driver,
                                              adxl345_status_t status)
{
  adxl345_mark_error(driver, status);
  if (driver->recovery_attempts >= ADXL345_MAX_RECOVERY_ATTEMPTS)
  {
    driver->status = ADXL345_STATUS_OFFLINE;
    driver->sample.status = ADXL345_STATUS_OFFLINE;
    driver->state = ADXL345_STATE_OFFLINE;
    return;
  }
  ++driver->recovery_attempts;
  driver->recovery_request_count =
      adxl345_saturating_increment(driver->recovery_request_count);
  driver->recovery_active = true;
  driver->status = ADXL345_STATUS_RECOVERY_REQUIRED;
  driver->sample.status = ADXL345_STATUS_RECOVERY_REQUIRED;
  driver->state = ADXL345_STATE_READ_ID;
}

static bool adxl345_record_transport(adxl345_t *driver,
                                     adxl345_transport_result_t result)
{
  driver->transaction_count =
      adxl345_saturating_increment(driver->transaction_count);
  driver->last_transport_result = result;
  if (result == ADXL345_TRANSPORT_OK)
  {
    return true;
  }
  adxl345_request_recovery_internal(driver, adxl345_map_transport(result));
  return false;
}

static bool adxl345_read_value(adxl345_t *driver,
                               uint8_t register_address,
                               uint8_t *value)
{
  return adxl345_record_transport(
      driver,
      driver->ops.read(driver->ops.context, register_address, value, 1U));
}

static bool adxl345_write_value(adxl345_t *driver,
                                uint8_t register_address,
                                uint8_t value)
{
  return adxl345_record_transport(
      driver,
      driver->ops.write(driver->ops.context, register_address, value));
}

static bool adxl345_verify_or_recover(adxl345_t *driver,
                                      uint8_t actual,
                                      uint8_t mask,
                                      uint8_t expected)
{
  if ((actual & mask) == expected)
  {
    return true;
  }
  adxl345_request_recovery_internal(
      driver, ADXL345_STATUS_CONFIGURATION_MISMATCH);
  return false;
}

bool adxl345_initialize(adxl345_t *driver,
                        const adxl345_ops_t *ops,
                        const adxl345_config_t *config)
{
  if ((driver == NULL) || (ops == NULL) || (config == NULL) ||
      (ops->read == NULL) || (ops->write == NULL))
  {
    return false;
  }
  (void)memset(driver, 0, sizeof(*driver));
  driver->ops = *ops;
  driver->config = *config;
  driver->state = ADXL345_STATE_READ_ID;
  driver->status = ADXL345_STATUS_INITIALIZING;
  driver->sample.status = ADXL345_STATUS_INITIALIZING;
  adxl345_feature_window_initialize(&driver->window);
  return true;
}

static void adxl345_note_events(adxl345_t *driver,
                                uint32_t event_count)
{
  driver->irq_event_count =
      adxl345_saturating_add(driver->irq_event_count, event_count);
  if (event_count > 1U)
  {
    const uint32_t dropped = event_count - 1U;
    driver->dropped_sample_lower_bound = adxl345_saturating_add(
        driver->dropped_sample_lower_bound, dropped);
    driver->pending_dropped_sample_lower_bound = adxl345_saturating_add(
        driver->pending_dropped_sample_lower_bound, dropped);
    driver->pending_gap = true;
  }
}

static void adxl345_publish_sample(adxl345_t *driver,
                                   const uint8_t data[ADXL345_DATA_LENGTH],
                                   uint32_t now_ms)
{
  adxl345_sample_t next = driver->sample;
  if (!adxl345_parse_xyz(data, next.raw_xyz))
  {
    adxl345_request_recovery_internal(
        driver, ADXL345_STATUS_TRANSPORT_INVALID_ARGUMENT);
    return;
  }
  next.status = ADXL345_STATUS_VALID;
  next.sequence = adxl345_saturating_increment(driver->sample.sequence);
  next.quality_flags = ADXL345_QUALITY_VALID;
  if (!driver->config.bias_calibrated)
  {
    next.quality_flags |= ADXL345_QUALITY_BIAS_UNCALIBRATED;
  }
  if (driver->pending_gap)
  {
    next.quality_flags |= ADXL345_QUALITY_GAP;
  }
  if (driver->pending_dropped_sample_lower_bound != 0U)
  {
    next.quality_flags |= ADXL345_QUALITY_DROPPED;
  }
  next.dropped_sample_lower_bound =
      driver->pending_dropped_sample_lower_bound;

  for (size_t axis = 0U; axis < ADXL345_AXIS_COUNT; ++axis)
  {
    next.corrected_counts[axis] =
        (int32_t)next.raw_xyz[axis] - driver->config.bias_lsb[axis];
    next.acceleration_millig[axis] =
        adxl345_counts_to_millig(next.corrected_counts[axis]);
  }

  driver->sample = next;
  driver->status = ADXL345_STATUS_VALID;
  driver->pending_gap = false;
  driver->pending_dropped_sample_lower_bound = 0U;
  driver->data_ready_deadline_ms = now_ms + ADXL345_DATA_READY_STALL_MS;
  (void)adxl345_feature_window_push(
      &driver->window, &driver->sample, &driver->feature);
  if (driver->recovery_active)
  {
    driver->recovery_success_count =
        adxl345_saturating_increment(driver->recovery_success_count);
    driver->recovery_active = false;
    driver->recovery_attempts = 0U;
  }
}

static bool adxl345_service_internal(adxl345_t *driver,
                                     uint32_t now_ms,
                                     uint32_t data_ready_event_count,
                                     bool count_as_irq)
{
  if ((driver == NULL) || (driver->ops.read == NULL) ||
      (driver->ops.write == NULL))
  {
    return false;
  }

  uint8_t value = 0U;
  switch (driver->state)
  {
    case ADXL345_STATE_READ_ID:
      if (adxl345_read_value(driver, ADXL345_DEVID_REGISTER, &value))
      {
        if (value == ADXL345_DEVID_VALUE)
        {
          driver->state = ADXL345_STATE_WRITE_STANDBY;
        }
        else
        {
          adxl345_mark_error(driver, ADXL345_STATUS_WRONG_ID);
          driver->state = ADXL345_STATE_OFFLINE;
        }
      }
      return true;

    case ADXL345_STATE_WRITE_STANDBY:
      if (adxl345_write_value(driver,
                              ADXL345_POWER_CTL_REGISTER,
                              ADXL345_POWER_CTL_STANDBY))
      {
        driver->state = ADXL345_STATE_DISABLE_INTERRUPTS;
      }
      return true;

    case ADXL345_STATE_DISABLE_INTERRUPTS:
      if (adxl345_write_value(driver, ADXL345_INT_ENABLE_REGISTER, 0U))
      {
        driver->state = ADXL345_STATE_SET_FIFO_BYPASS;
      }
      return true;

    case ADXL345_STATE_SET_FIFO_BYPASS:
      if (adxl345_write_value(driver,
                              ADXL345_FIFO_CTL_REGISTER,
                              ADXL345_FIFO_BYPASS))
      {
        driver->state = ADXL345_STATE_SET_DATA_FORMAT;
      }
      return true;

    case ADXL345_STATE_SET_DATA_FORMAT:
      if (adxl345_write_value(driver,
                              ADXL345_DATA_FORMAT_REGISTER,
                              ADXL345_DATA_FORMAT_FULL_RES_4G))
      {
        driver->state = ADXL345_STATE_SET_BW_RATE;
      }
      return true;

    case ADXL345_STATE_SET_BW_RATE:
      if (adxl345_write_value(driver,
                              ADXL345_BW_RATE_REGISTER,
                              ADXL345_BW_RATE_100_HZ))
      {
        driver->state = ADXL345_STATE_MAP_INT1;
      }
      return true;

    case ADXL345_STATE_MAP_INT1:
      if (adxl345_write_value(driver,
                              ADXL345_INT_MAP_REGISTER,
                              ADXL345_INT_MAP_DATA_READY_TARGET))
      {
        driver->state = ADXL345_STATE_VERIFY_DATA_FORMAT;
      }
      return true;

    case ADXL345_STATE_VERIFY_DATA_FORMAT:
      if (adxl345_read_value(driver, ADXL345_DATA_FORMAT_REGISTER, &value) &&
          adxl345_verify_or_recover(driver,
                                    value,
                                    ADXL345_DATA_FORMAT_VERIFY_MASK,
                                    ADXL345_DATA_FORMAT_FULL_RES_4G))
      {
        driver->state = ADXL345_STATE_VERIFY_BW_RATE;
      }
      return true;

    case ADXL345_STATE_VERIFY_BW_RATE:
      if (adxl345_read_value(driver, ADXL345_BW_RATE_REGISTER, &value) &&
          adxl345_verify_or_recover(driver,
                                    value,
                                    ADXL345_BW_RATE_VERIFY_MASK,
                                    ADXL345_BW_RATE_100_HZ))
      {
        driver->state = ADXL345_STATE_VERIFY_INT_MAP;
      }
      return true;

    case ADXL345_STATE_VERIFY_INT_MAP:
      if (adxl345_read_value(driver, ADXL345_INT_MAP_REGISTER, &value) &&
          adxl345_verify_or_recover(
              driver,
              value,
              ADXL345_INT_DATA_READY,
              ADXL345_INT_MAP_DATA_READY_TARGET))
      {
        driver->state = ADXL345_STATE_ENTER_MEASURE;
      }
      return true;

    case ADXL345_STATE_ENTER_MEASURE:
      if (adxl345_write_value(driver,
                              ADXL345_POWER_CTL_REGISTER,
                              ADXL345_POWER_CTL_MEASURE))
      {
        driver->state = ADXL345_STATE_VERIFY_POWER_CTL;
      }
      return true;

    case ADXL345_STATE_VERIFY_POWER_CTL:
      if (adxl345_read_value(driver, ADXL345_POWER_CTL_REGISTER, &value) &&
          adxl345_verify_or_recover(driver,
                                    value,
                                    ADXL345_POWER_CTL_MEASURE,
                                    ADXL345_POWER_CTL_MEASURE))
      {
        driver->state = ADXL345_STATE_ENABLE_DATA_READY;
      }
      return true;

    case ADXL345_STATE_ENABLE_DATA_READY:
      if (adxl345_write_value(driver,
                              ADXL345_INT_ENABLE_REGISTER,
                              ADXL345_INT_DATA_READY))
      {
        driver->state = ADXL345_STATE_WAIT_DATA_READY;
        driver->data_ready_deadline_ms =
            now_ms + ADXL345_DATA_READY_STALL_MS;
      }
      return true;

    case ADXL345_STATE_WAIT_DATA_READY:
      if (data_ready_event_count != 0U)
      {
        uint8_t data[ADXL345_DATA_LENGTH];
        if (count_as_irq)
        {
          adxl345_note_events(driver, data_ready_event_count);
        }
        const adxl345_transport_result_t result = driver->ops.read(
            driver->ops.context,
            ADXL345_DATAX0_REGISTER,
            data,
            sizeof(data));
        if (adxl345_record_transport(driver, result))
        {
          adxl345_publish_sample(driver, data, now_ms);
        }
        else
        {
          driver->dropped_sample_lower_bound = adxl345_saturating_increment(
              driver->dropped_sample_lower_bound);
          driver->pending_dropped_sample_lower_bound =
              adxl345_saturating_increment(
                  driver->pending_dropped_sample_lower_bound);
        }
      }
      else if (adxl345_time_reached(now_ms,
                                    driver->data_ready_deadline_ms))
      {
        adxl345_request_recovery_internal(
            driver, ADXL345_STATUS_DATA_READY_STALLED);
      }
      return true;

    case ADXL345_STATE_UNINITIALIZED:
    case ADXL345_STATE_OFFLINE:
    default:
      return true;
  }
}

bool adxl345_service(adxl345_t *driver,
                     uint32_t now_ms,
                     uint32_t data_ready_event_count)
{
  return adxl345_service_internal(
      driver, now_ms, data_ready_event_count, true);
}

bool adxl345_service_polled_data_ready(adxl345_t *driver,
                                       uint32_t now_ms)
{
  return adxl345_service_internal(driver, now_ms, 1U, false);
}

bool adxl345_request_reinitialize(adxl345_t *driver)
{
  if (driver == NULL)
  {
    return false;
  }
  driver->state = ADXL345_STATE_READ_ID;
  driver->status = ADXL345_STATUS_INITIALIZING;
  driver->sample.status = ADXL345_STATUS_INITIALIZING;
  driver->sample.quality_flags = 0U;
  driver->recovery_attempts = 0U;
  driver->recovery_active = false;
  driver->pending_gap = true;
  adxl345_feature_window_initialize(&driver->window);
  return true;
}

bool adxl345_get_sample(const adxl345_t *driver, adxl345_sample_t *sample)
{
  if ((driver == NULL) || (sample == NULL))
  {
    return false;
  }
  *sample = driver->sample;
  return true;
}

bool adxl345_get_feature(const adxl345_t *driver,
                         adxl345_feature_t *feature)
{
  if ((driver == NULL) || (feature == NULL))
  {
    return false;
  }
  *feature = driver->feature;
  return true;
}
