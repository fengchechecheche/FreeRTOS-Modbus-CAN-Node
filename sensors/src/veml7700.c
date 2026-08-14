#include "veml7700.h"

#include <limits.h>
#include <string.h>

#define VEML7700_CONFIG_WRITABLE_MASK UINT16_C(0x1bf3)
#define VEML7700_MAX_RANGING_ADJUSTMENTS (8U)
#define VEML7700_MAX_RECOVERY_ATTEMPTS (1U)
#define VEML7700_SERVICE_PERIOD_MS UINT32_C(20)

static const veml7700_range_config_t veml7700_range_table[] = {
    {UINT16_C(0x1300), UINT16_C(25), UINT16_C(21504),
     VEML7700_GAIN_ONE_EIGHTH},
    {UINT16_C(0x1200), UINT16_C(50), UINT16_C(10752),
     VEML7700_GAIN_ONE_EIGHTH},
    {UINT16_C(0x1000), UINT16_C(100), UINT16_C(5376),
     VEML7700_GAIN_ONE_EIGHTH},
    {UINT16_C(0x1800), UINT16_C(100), UINT16_C(2688),
     VEML7700_GAIN_ONE_QUARTER},
    {UINT16_C(0x0000), UINT16_C(100), UINT16_C(672),
     VEML7700_GAIN_ONE},
    {UINT16_C(0x0800), UINT16_C(100), UINT16_C(336),
     VEML7700_GAIN_TWO},
    {UINT16_C(0x0840), UINT16_C(200), UINT16_C(168),
     VEML7700_GAIN_TWO},
    {UINT16_C(0x0880), UINT16_C(400), UINT16_C(84),
     VEML7700_GAIN_TWO},
    {UINT16_C(0x08c0), UINT16_C(800), UINT16_C(42),
     VEML7700_GAIN_TWO},
};

_Static_assert((sizeof(veml7700_range_table) /
                sizeof(veml7700_range_table[0])) ==
                   VEML7700_RANGE_LEVEL_COUNT,
               "VEML7700 range table count drifted");

static uint32_t veml7700_saturating_increment(uint32_t value)
{
  return value == UINT32_MAX ? value : value + 1U;
}

static bool veml7700_time_reached(uint32_t now_ms, uint32_t target_ms)
{
  return (int32_t)(now_ms - target_ms) >= 0;
}

static uint32_t veml7700_integration_wait_ms(uint16_t integration_ms)
{
  const uint32_t tolerance_wait =
      (((uint32_t)integration_ms * UINT32_C(13)) + UINT32_C(9)) /
      UINT32_C(10);
  return ((tolerance_wait + VEML7700_SERVICE_PERIOD_MS - 1U) /
          VEML7700_SERVICE_PERIOD_MS) *
         VEML7700_SERVICE_PERIOD_MS;
}

bool veml7700_get_range_config(uint8_t range_level,
                               veml7700_range_config_t *config)
{
  if ((config == NULL) ||
      (range_level >= (uint8_t)VEML7700_RANGE_LEVEL_COUNT))
  {
    return false;
  }
  *config = veml7700_range_table[range_level];
  return true;
}

uint16_t veml7700_decode_word(
    const uint8_t data[VEML7700_WORD_LENGTH])
{
  if (data == NULL)
  {
    return 0U;
  }
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

bool veml7700_encode_word(uint16_t value,
                          uint8_t data[VEML7700_WORD_LENGTH])
{
  if (data == NULL)
  {
    return false;
  }
  data[0] = (uint8_t)(value & UINT16_C(0x00ff));
  data[1] = (uint8_t)(value >> 8U);
  return true;
}

bool veml7700_convert_millilux(uint16_t raw_als,
                               uint8_t range_level,
                               uint32_t *illuminance_millilux)
{
  veml7700_range_config_t range;
  if ((illuminance_millilux == NULL) ||
      !veml7700_get_range_config(range_level, &range))
  {
    return false;
  }

  const uint64_t scaled =
      (uint64_t)raw_als * range.resolution_0p0001_lux;
  *illuminance_millilux = (uint32_t)((scaled + UINT64_C(5)) /
                                     UINT64_C(10));
  return true;
}

static veml7700_status_t veml7700_map_transport_status(
    veml7700_transport_result_t result)
{
  switch (result)
  {
    case VEML7700_TRANSPORT_INVALID_ARGUMENT:
      return VEML7700_STATUS_TRANSPORT_INVALID_ARGUMENT;
    case VEML7700_TRANSPORT_NOT_PRESENT:
      return VEML7700_STATUS_NOT_PRESENT;
    case VEML7700_TRANSPORT_BUSY:
      return VEML7700_STATUS_TRANSPORT_BUSY;
    case VEML7700_TRANSPORT_TIMEOUT:
      return VEML7700_STATUS_TRANSPORT_TIMEOUT;
    case VEML7700_TRANSPORT_IO_ERROR:
    default:
      return VEML7700_STATUS_TRANSPORT_IO_ERROR;
  }
}

static void veml7700_mark_error(veml7700_t *driver,
                                veml7700_status_t status)
{
  driver->last_error_status = status;
  driver->status = status;
  driver->sample.status = status;
  driver->sample.quality_flags &= ~VEML7700_QUALITY_VALID;
  driver->error_count = veml7700_saturating_increment(driver->error_count);
}

static void veml7700_request_recovery_internal(
    veml7700_t *driver,
    veml7700_status_t status)
{
  veml7700_mark_error(driver, status);
  if (driver->recovery_attempts >= VEML7700_MAX_RECOVERY_ATTEMPTS)
  {
    driver->status = VEML7700_STATUS_OFFLINE;
    driver->sample.status = VEML7700_STATUS_OFFLINE;
    driver->state = VEML7700_STATE_OFFLINE;
    return;
  }

  ++driver->recovery_attempts;
  driver->recovery_request_count =
      veml7700_saturating_increment(driver->recovery_request_count);
  driver->recovery_active = true;
  driver->status = VEML7700_STATUS_RECOVERY_REQUIRED;
  driver->sample.status = VEML7700_STATUS_RECOVERY_REQUIRED;
  if ((driver->ops.request_recovery != NULL) &&
      driver->ops.request_recovery(driver->ops.context))
  {
    driver->state = VEML7700_STATE_READ_CONFIG;
    return;
  }

  driver->status = VEML7700_STATUS_RECOVERY_UNAVAILABLE;
  driver->sample.status = VEML7700_STATUS_RECOVERY_UNAVAILABLE;
  driver->state = VEML7700_STATE_OFFLINE;
}

static bool veml7700_record_transport(veml7700_t *driver,
                                      veml7700_transport_result_t result)
{
  driver->transaction_count =
      veml7700_saturating_increment(driver->transaction_count);
  driver->last_transport_result = result;
  if (result == VEML7700_TRANSPORT_OK)
  {
    return true;
  }
  veml7700_request_recovery_internal(
      driver, veml7700_map_transport_status(result));
  return false;
}

static bool veml7700_change_range(veml7700_t *driver, bool increase)
{
  if (driver->ranging_adjustments >= VEML7700_MAX_RANGING_ADJUSTMENTS)
  {
    return false;
  }
  if (increase)
  {
    if (driver->range_level >= (VEML7700_RANGE_LEVEL_COUNT - 1U))
    {
      return false;
    }
    ++driver->range_level;
  }
  else
  {
    if (driver->range_level == 0U)
    {
      return false;
    }
    --driver->range_level;
  }

  ++driver->ranging_adjustments;
  driver->range_change_count =
      veml7700_saturating_increment(driver->range_change_count);
  driver->status = VEML7700_STATUS_RANGING;
  driver->sample.status = VEML7700_STATUS_RANGING;
  driver->sample.quality_flags = VEML7700_QUALITY_RANGING;
  driver->state = VEML7700_STATE_WRITE_CONFIG;
  return true;
}

bool veml7700_initialize(veml7700_t *driver,
                         const veml7700_ops_t *ops,
                         uint32_t sample_period_ms)
{
  if ((driver == NULL) || (ops == NULL) || (ops->read == NULL) ||
      (ops->write == NULL) || (sample_period_ms == 0U))
  {
    return false;
  }

  (void)memset(driver, 0, sizeof(*driver));
  driver->ops = *ops;
  driver->sample_period_ms = sample_period_ms;
  driver->range_level = VEML7700_DEFAULT_RANGE_LEVEL;
  driver->state = VEML7700_STATE_READ_CONFIG;
  driver->status = VEML7700_STATUS_INITIALIZING;
  driver->sample.status = VEML7700_STATUS_INITIALIZING;
  return true;
}

bool veml7700_service(veml7700_t *driver, uint32_t now_ms)
{
  if ((driver == NULL) || (driver->ops.read == NULL) ||
      (driver->ops.write == NULL))
  {
    return false;
  }

  veml7700_range_config_t range;
  if (!veml7700_get_range_config(driver->range_level, &range))
  {
    veml7700_mark_error(driver, VEML7700_STATUS_CONFIGURATION_MISMATCH);
    driver->state = VEML7700_STATE_OFFLINE;
    return true;
  }

  uint8_t data[VEML7700_WORD_LENGTH] = {0U, 0U};
  veml7700_transport_result_t result;
  switch (driver->state)
  {
    case VEML7700_STATE_READ_CONFIG:
      result = driver->ops.read(driver->ops.context,
                                VEML7700_CONFIG_REGISTER,
                                data,
                                sizeof(data));
      if (veml7700_record_transport(driver, result))
      {
        driver->state = VEML7700_STATE_WRITE_CONFIG;
      }
      return true;

    case VEML7700_STATE_WRITE_CONFIG:
      (void)veml7700_encode_word(range.config_word, data);
      result = driver->ops.write(driver->ops.context,
                                 VEML7700_CONFIG_REGISTER,
                                 data,
                                 sizeof(data));
      if (veml7700_record_transport(driver, result))
      {
        driver->state = VEML7700_STATE_VERIFY_CONFIG;
      }
      return true;

    case VEML7700_STATE_VERIFY_CONFIG:
      result = driver->ops.read(driver->ops.context,
                                VEML7700_CONFIG_REGISTER,
                                data,
                                sizeof(data));
      if (!veml7700_record_transport(driver, result))
      {
        return true;
      }
      if ((veml7700_decode_word(data) & VEML7700_CONFIG_WRITABLE_MASK) !=
          (range.config_word & VEML7700_CONFIG_WRITABLE_MASK))
      {
        veml7700_request_recovery_internal(
            driver, VEML7700_STATUS_CONFIGURATION_MISMATCH);
        return true;
      }
      if (driver->recovery_active)
      {
        driver->recovery_success_count =
            veml7700_saturating_increment(driver->recovery_success_count);
        driver->recovery_active = false;
        driver->recovery_attempts = 0U;
      }
      driver->status = VEML7700_STATUS_RANGING;
      driver->sample.status = VEML7700_STATUS_RANGING;
      driver->sample.quality_flags = VEML7700_QUALITY_RANGING;
      driver->next_action_ms =
          now_ms + veml7700_integration_wait_ms(range.integration_ms);
      driver->state = VEML7700_STATE_WAIT_INTEGRATION;
      return true;

    case VEML7700_STATE_WAIT_INTEGRATION:
      if (veml7700_time_reached(now_ms, driver->next_action_ms))
      {
        driver->state = VEML7700_STATE_READ_ALS;
      }
      return true;

    case VEML7700_STATE_READ_ALS:
    {
      result = driver->ops.read(driver->ops.context,
                                VEML7700_ALS_REGISTER,
                                data,
                                sizeof(data));
      if (!veml7700_record_transport(driver, result))
      {
        return true;
      }

      const uint16_t raw_als = veml7700_decode_word(data);
      if ((raw_als <= VEML7700_LOW_COUNT_THRESHOLD) &&
          veml7700_change_range(driver, true))
      {
        return true;
      }
      if ((raw_als >= VEML7700_HIGH_COUNT_THRESHOLD) &&
          veml7700_change_range(driver, false))
      {
        return true;
      }

      veml7700_sample_t next_sample = driver->sample;
      if (!veml7700_convert_millilux(raw_als,
                                     driver->range_level,
                                     &next_sample.illuminance_millilux))
      {
        veml7700_mark_error(
            driver, VEML7700_STATUS_CONFIGURATION_MISMATCH);
        driver->state = VEML7700_STATE_OFFLINE;
        return true;
      }
      next_sample.raw_als = raw_als;
      next_sample.range_level = driver->range_level;
      next_sample.gain = range.gain;
      next_sample.integration_ms = range.integration_ms;
      next_sample.sequence =
          veml7700_saturating_increment(driver->sample.sequence);
      next_sample.quality_flags = VEML7700_QUALITY_VALID;
      if (raw_als <= VEML7700_LOW_COUNT_THRESHOLD)
      {
        next_sample.quality_flags |= VEML7700_QUALITY_LOW_COUNT |
                                     VEML7700_QUALITY_RANGE_LIMITED;
      }
      if (raw_als >= VEML7700_HIGH_COUNT_THRESHOLD)
      {
        next_sample.quality_flags |= VEML7700_QUALITY_RANGE_LIMITED;
      }
      if (raw_als >= VEML7700_SATURATION_THRESHOLD)
      {
        next_sample.quality_flags |= VEML7700_QUALITY_SATURATED;
      }
      if (next_sample.illuminance_millilux >
          VEML7700_HIGH_LUX_UNCORRECTED_MILLILUX)
      {
        next_sample.quality_flags |=
            VEML7700_QUALITY_HIGH_LUX_UNCORRECTED;
      }

      if ((next_sample.quality_flags & VEML7700_QUALITY_SATURATED) != 0U)
      {
        next_sample.status = VEML7700_STATUS_SATURATED;
      }
      else if ((next_sample.quality_flags &
                VEML7700_QUALITY_RANGE_LIMITED) != 0U)
      {
        next_sample.status = VEML7700_STATUS_RANGE_LIMITED;
      }
      else
      {
        next_sample.status = VEML7700_STATUS_VALID;
      }
      driver->sample = next_sample;
      driver->status = next_sample.status;
      driver->ranging_adjustments = 0U;
      driver->recovery_attempts = 0U;
      driver->state = VEML7700_STATE_IDLE;
      driver->next_action_ms = now_ms + driver->sample_period_ms;
      return true;
    }

    case VEML7700_STATE_IDLE:
      if (veml7700_time_reached(now_ms, driver->next_action_ms))
      {
        driver->state = VEML7700_STATE_READ_ALS;
      }
      return true;

    case VEML7700_STATE_UNINITIALIZED:
    case VEML7700_STATE_OFFLINE:
    default:
      return true;
  }
}

bool veml7700_request_reinitialize(veml7700_t *driver)
{
  if (driver == NULL)
  {
    return false;
  }
  driver->range_level = VEML7700_DEFAULT_RANGE_LEVEL;
  driver->ranging_adjustments = 0U;
  driver->recovery_attempts = 0U;
  driver->recovery_active = false;
  driver->state = VEML7700_STATE_READ_CONFIG;
  driver->status = VEML7700_STATUS_INITIALIZING;
  driver->sample.status = VEML7700_STATUS_INITIALIZING;
  driver->sample.quality_flags = 0U;
  return true;
}

bool veml7700_get_sample(const veml7700_t *driver,
                         veml7700_sample_t *sample)
{
  if ((driver == NULL) || (sample == NULL))
  {
    return false;
  }
  *sample = driver->sample;
  return true;
}
