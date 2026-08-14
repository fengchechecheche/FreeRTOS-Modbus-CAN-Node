#include "bme280.h"

#include <limits.h>
#include <string.h>

#define BME280_SKIPPED_20BIT UINT32_C(0x80000)
#define BME280_SKIPPED_HUMIDITY UINT16_C(0x8000)
#define BME280_MAX_RECOVERY_ATTEMPTS (1U)
#define BME280_MIN_TEMPERATURE_CENTI_C INT32_C(-4000)
#define BME280_MAX_TEMPERATURE_CENTI_C INT32_C(8500)
#define BME280_MIN_PRESSURE_PA UINT32_C(30000)
#define BME280_MAX_PRESSURE_PA UINT32_C(110000)

static uint32_t bme280_saturating_increment(uint32_t value)
{
  return value == UINT32_MAX ? value : value + 1U;
}

static bool bme280_time_reached(uint32_t now_ms, uint32_t target_ms)
{
  return (int32_t)(now_ms - target_ms) >= 0;
}

static uint16_t bme280_read_u16(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

static int16_t bme280_read_s16(const uint8_t *data)
{
  const uint16_t raw = bme280_read_u16(data);
  if (raw <= (uint16_t)INT16_MAX)
  {
    return (int16_t)raw;
  }
  return (int16_t)((int32_t)raw - INT32_C(65536));
}

static int16_t bme280_sign_extend_12(uint16_t raw)
{
  raw &= UINT16_C(0x0fff);
  if ((raw & UINT16_C(0x0800)) != 0U)
  {
    return (int16_t)((int32_t)raw - INT32_C(4096));
  }
  return (int16_t)raw;
}

static bool bme280_all_equal(const uint8_t *data,
                             size_t length,
                             uint8_t value)
{
  for (size_t index = 0U; index < length; ++index)
  {
    if (data[index] != value)
    {
      return false;
    }
  }
  return true;
}

bool bme280_parse_calibration(
    const uint8_t calibration_1[BME280_CALIBRATION_1_LENGTH],
    const uint8_t calibration_2[BME280_CALIBRATION_2_LENGTH],
    bme280_calibration_t *calibration)
{
  if ((calibration_1 == NULL) || (calibration_2 == NULL) ||
      (calibration == NULL))
  {
    return false;
  }

  calibration->dig_t1 = bme280_read_u16(&calibration_1[0]);
  calibration->dig_t2 = bme280_read_s16(&calibration_1[2]);
  calibration->dig_t3 = bme280_read_s16(&calibration_1[4]);
  calibration->dig_p1 = bme280_read_u16(&calibration_1[6]);
  calibration->dig_p2 = bme280_read_s16(&calibration_1[8]);
  calibration->dig_p3 = bme280_read_s16(&calibration_1[10]);
  calibration->dig_p4 = bme280_read_s16(&calibration_1[12]);
  calibration->dig_p5 = bme280_read_s16(&calibration_1[14]);
  calibration->dig_p6 = bme280_read_s16(&calibration_1[16]);
  calibration->dig_p7 = bme280_read_s16(&calibration_1[18]);
  calibration->dig_p8 = bme280_read_s16(&calibration_1[20]);
  calibration->dig_p9 = bme280_read_s16(&calibration_1[22]);
  calibration->dig_h1 = calibration_1[25];
  calibration->dig_h2 = bme280_read_s16(&calibration_2[0]);
  calibration->dig_h3 = calibration_2[2];
  calibration->dig_h4 = bme280_sign_extend_12(
      (uint16_t)(((uint16_t)calibration_2[3] << 4U) |
                 ((uint16_t)calibration_2[4] & UINT16_C(0x000f))));
  calibration->dig_h5 = bme280_sign_extend_12(
      (uint16_t)(((uint16_t)calibration_2[5] << 4U) |
                 ((uint16_t)calibration_2[4] >> 4U)));
  calibration->dig_h6 = calibration_2[6] <= (uint8_t)INT8_MAX
                            ? (int8_t)calibration_2[6]
                            : (int8_t)((int16_t)calibration_2[6] - 256);
  return true;
}

bool bme280_calibration_is_valid(
    const uint8_t calibration_1[BME280_CALIBRATION_1_LENGTH],
    const uint8_t calibration_2[BME280_CALIBRATION_2_LENGTH],
    const bme280_calibration_t *calibration)
{
  if ((calibration_1 == NULL) || (calibration_2 == NULL) ||
      (calibration == NULL))
  {
    return false;
  }

  const bool all_zero =
      bme280_all_equal(calibration_1, BME280_CALIBRATION_1_LENGTH, 0U) &&
      bme280_all_equal(calibration_2, BME280_CALIBRATION_2_LENGTH, 0U);
  const bool all_ff =
      bme280_all_equal(calibration_1,
                       BME280_CALIBRATION_1_LENGTH,
                       UINT8_MAX) &&
      bme280_all_equal(calibration_2,
                       BME280_CALIBRATION_2_LENGTH,
                       UINT8_MAX);
  return !all_zero && !all_ff && (calibration->dig_t1 != 0U) &&
         (calibration->dig_p1 != 0U);
}

bool bme280_parse_raw(const uint8_t data[BME280_RAW_DATA_LENGTH],
                      bme280_raw_sample_t *raw)
{
  if ((data == NULL) || (raw == NULL))
  {
    return false;
  }

  raw->pressure = ((uint32_t)data[0] << 12U) |
                  ((uint32_t)data[1] << 4U) |
                  ((uint32_t)data[2] >> 4U);
  raw->temperature = ((uint32_t)data[3] << 12U) |
                     ((uint32_t)data[4] << 4U) |
                     ((uint32_t)data[5] >> 4U);
  raw->humidity =
      (uint16_t)(((uint16_t)data[6] << 8U) | (uint16_t)data[7]);
  return true;
}

bool bme280_compensate(const bme280_calibration_t *calibration,
                       const bme280_raw_sample_t *raw,
                       bme280_sample_t *sample)
{
  if ((calibration == NULL) || (raw == NULL) || (sample == NULL) ||
      (calibration->dig_t1 == 0U) || (calibration->dig_p1 == 0U) ||
      (raw->temperature == BME280_SKIPPED_20BIT) ||
      (raw->pressure == BME280_SKIPPED_20BIT) ||
      (raw->humidity == BME280_SKIPPED_HUMIDITY))
  {
    return false;
  }

  const int32_t temperature_delta_1 =
      (int32_t)(raw->temperature >> 3U) -
      ((int32_t)calibration->dig_t1 * 2);
  const int32_t temperature_delta_2 =
      (int32_t)(raw->temperature >> 4U) -
      (int32_t)calibration->dig_t1;
  const int32_t temperature_var1 =
      (int32_t)(((int64_t)temperature_delta_1 * calibration->dig_t2) /
                2048);
  const int64_t temperature_square =
      (int64_t)temperature_delta_2 * temperature_delta_2;
  const int32_t temperature_var2 =
      (int32_t)(((temperature_square / 4096) * calibration->dig_t3) /
                16384);
  const int32_t t_fine = temperature_var1 + temperature_var2;
  int32_t temperature_centi_c = (t_fine * 5 + 128) / 256;
  if (temperature_centi_c < BME280_MIN_TEMPERATURE_CENTI_C)
  {
    temperature_centi_c = BME280_MIN_TEMPERATURE_CENTI_C;
  }
  else if (temperature_centi_c > BME280_MAX_TEMPERATURE_CENTI_C)
  {
    temperature_centi_c = BME280_MAX_TEMPERATURE_CENTI_C;
  }

  int64_t pressure_var1 = (int64_t)t_fine - INT64_C(128000);
  int64_t pressure_var2 =
      pressure_var1 * pressure_var1 * calibration->dig_p6;
  pressure_var2 += pressure_var1 * calibration->dig_p5 * INT64_C(131072);
  pressure_var2 += (int64_t)calibration->dig_p4 * INT64_C(34359738368);
  pressure_var1 =
      ((pressure_var1 * pressure_var1 * calibration->dig_p3) / 256) +
      (pressure_var1 * calibration->dig_p2 * INT64_C(4096));
  pressure_var1 =
      (((INT64_C(1) << 47) + pressure_var1) * calibration->dig_p1) /
      INT64_C(8589934592);
  if (pressure_var1 == 0)
  {
    return false;
  }

  int64_t pressure_q24_8 = INT64_C(1048576) - (int64_t)raw->pressure;
  pressure_q24_8 =
      ((pressure_q24_8 * INT64_C(2147483648) - pressure_var2) * 3125) /
      pressure_var1;
  pressure_var1 =
      ((int64_t)calibration->dig_p9 * (pressure_q24_8 / 8192) *
       (pressure_q24_8 / 8192)) /
      INT64_C(33554432);
  pressure_var2 =
      ((int64_t)calibration->dig_p8 * pressure_q24_8) / INT64_C(524288);
  pressure_q24_8 =
      ((pressure_q24_8 + pressure_var1 + pressure_var2) / 256) +
      ((int64_t)calibration->dig_p7 * 16);
  if ((pressure_q24_8 < 0) ||
      (pressure_q24_8 > ((int64_t)UINT32_MAX * 256)))
  {
    return false;
  }

  int32_t humidity = t_fine - INT32_C(76800);
  int64_t humidity_left =
      ((int64_t)raw->humidity * INT64_C(16384)) -
      ((int64_t)calibration->dig_h4 * INT64_C(1048576)) -
      ((int64_t)calibration->dig_h5 * humidity) + INT64_C(16384);
  humidity_left /= INT64_C(32768);
  const int64_t humidity_h6_term =
      ((int64_t)humidity * calibration->dig_h6) / 1024;
  const int64_t humidity_h3_term =
      ((int64_t)humidity * calibration->dig_h3) / 2048;
  int64_t humidity_right =
      (humidity_h6_term * (humidity_h3_term + 32768)) / 1024 +
      INT64_C(2097152);
  humidity_right =
      (humidity_right * calibration->dig_h2 + INT64_C(8192)) /
      INT64_C(16384);
  int64_t humidity_value = humidity_left * humidity_right;
  humidity_value -=
      (((((humidity_value / INT64_C(32768)) *
          (humidity_value / INT64_C(32768))) /
         128) *
        calibration->dig_h1) /
       16);
  if (humidity_value < 0)
  {
    humidity_value = 0;
  }
  if (humidity_value > INT64_C(419430400))
  {
    humidity_value = INT64_C(419430400);
  }
  const uint32_t humidity_q22_10 =
      (uint32_t)(humidity_value / INT64_C(4096));

  sample->raw_temperature = raw->temperature;
  sample->raw_pressure = raw->pressure;
  sample->raw_humidity = raw->humidity;
  sample->temperature_centi_c = temperature_centi_c;
  uint32_t pressure_pa = (uint32_t)((pressure_q24_8 + 128) / 256);
  if (pressure_pa < BME280_MIN_PRESSURE_PA)
  {
    pressure_pa = BME280_MIN_PRESSURE_PA;
  }
  else if (pressure_pa > BME280_MAX_PRESSURE_PA)
  {
    pressure_pa = BME280_MAX_PRESSURE_PA;
  }
  sample->pressure_pa = pressure_pa;
  sample->humidity_milli_pct =
      (uint32_t)(((uint64_t)humidity_q22_10 * 1000U + 512U) / 1024U);
  sample->valid_mask = BME280_SAMPLE_VALID_ALL;
  sample->status = BME280_STATUS_VALID;
  return true;
}

static bme280_status_t bme280_map_transport_result(
    bme280_transport_result_t result)
{
  switch (result)
  {
    case BME280_TRANSPORT_INVALID_ARGUMENT:
      return BME280_STATUS_TRANSPORT_INVALID_ARGUMENT;
    case BME280_TRANSPORT_BUSY:
      return BME280_STATUS_TRANSPORT_BUSY;
    case BME280_TRANSPORT_TIMEOUT:
      return BME280_STATUS_TRANSPORT_TIMEOUT;
    case BME280_TRANSPORT_IO_ERROR:
    default:
      return BME280_STATUS_TRANSPORT_IO_ERROR;
  }
}

static void bme280_mark_error(bme280_t *driver, bme280_status_t status)
{
  driver->status = status;
  driver->sample.status = status;
  driver->sample.valid_mask = 0U;
  driver->error_count = bme280_saturating_increment(driver->error_count);
}

static void bme280_request_recovery_internal(bme280_t *driver,
                                             bme280_status_t status)
{
  bme280_mark_error(driver, status);
  if (driver->recovery_attempts < BME280_MAX_RECOVERY_ATTEMPTS)
  {
    ++driver->recovery_attempts;
    driver->recovery_request_count =
        bme280_saturating_increment(driver->recovery_request_count);
    driver->recovery_active = true;
    driver->status = BME280_STATUS_RECOVERY_REQUIRED;
    driver->sample.status = BME280_STATUS_RECOVERY_REQUIRED;
    driver->state = BME280_STATE_SOFT_RESET;
  }
  else
  {
    driver->status = BME280_STATUS_OFFLINE;
    driver->sample.status = BME280_STATUS_OFFLINE;
    driver->state = BME280_STATE_OFFLINE;
  }
}

static bool bme280_record_transport(bme280_t *driver,
                                    bme280_transport_result_t result)
{
  driver->transaction_count =
      bme280_saturating_increment(driver->transaction_count);
  driver->last_transport_result = result;
  if (result == BME280_TRANSPORT_OK)
  {
    return true;
  }
  bme280_request_recovery_internal(driver,
                                   bme280_map_transport_result(result));
  return false;
}

bool bme280_initialize(bme280_t *driver,
                       const bme280_ops_t *ops,
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
  driver->state = BME280_STATE_READ_ID;
  driver->status = BME280_STATUS_INITIALIZING;
  driver->sample.status = BME280_STATUS_INITIALIZING;
  return true;
}

bool bme280_service(bme280_t *driver, uint32_t now_ms)
{
  if ((driver == NULL) || (driver->ops.read == NULL) ||
      (driver->ops.write == NULL))
  {
    return false;
  }

  uint8_t value = 0U;
  bme280_transport_result_t result;
  switch (driver->state)
  {
    case BME280_STATE_READ_ID:
      result = driver->ops.read(driver->ops.context,
                                BME280_CHIP_ID_REGISTER,
                                &value,
                                1U);
      if (!bme280_record_transport(driver, result))
      {
        return true;
      }
      if (value == BME280_CHIP_ID)
      {
        driver->state = BME280_STATE_SOFT_RESET;
      }
      else
      {
        bme280_mark_error(
            driver,
            value == BME280_BMP280_CHIP_ID
                ? BME280_STATUS_UNSUPPORTED_BMP280
                : BME280_STATUS_WRONG_ID);
        driver->state = BME280_STATE_OFFLINE;
      }
      return true;

    case BME280_STATE_SOFT_RESET:
      result = driver->ops.write(driver->ops.context,
                                 BME280_RESET_REGISTER,
                                 BME280_RESET_COMMAND);
      if (!bme280_record_transport(driver, result))
      {
        return true;
      }
      driver->state = BME280_STATE_WAIT_NVM;
      driver->next_action_ms = now_ms + BME280_RESET_READY_DELAY_MS;
      driver->deadline_ms = now_ms + BME280_RESET_DEADLINE_MS;
      return true;

    case BME280_STATE_WAIT_NVM:
      if (!bme280_time_reached(now_ms, driver->next_action_ms))
      {
        return true;
      }
      if (bme280_time_reached(now_ms, driver->deadline_ms))
      {
        bme280_request_recovery_internal(driver, BME280_STATUS_NVM_TIMEOUT);
        return true;
      }
      result = driver->ops.read(driver->ops.context,
                                BME280_STATUS_REGISTER,
                                &value,
                                1U);
      if (!bme280_record_transport(driver, result))
      {
        return true;
      }
      if ((value & BME280_STATUS_IM_UPDATE_MASK) == 0U)
      {
        driver->state = BME280_STATE_READ_CALIBRATION_1;
      }
      else
      {
        driver->status = BME280_STATUS_NOT_READY;
        driver->sample.status = BME280_STATUS_NOT_READY;
        driver->next_action_ms = now_ms + BME280_RESET_READY_DELAY_MS;
      }
      return true;

    case BME280_STATE_READ_CALIBRATION_1:
      result = driver->ops.read(driver->ops.context,
                                BME280_CALIBRATION_1_REGISTER,
                                driver->calibration_1,
                                BME280_CALIBRATION_1_LENGTH);
      if (bme280_record_transport(driver, result))
      {
        driver->state = BME280_STATE_READ_CALIBRATION_2;
      }
      return true;

    case BME280_STATE_READ_CALIBRATION_2:
      result = driver->ops.read(driver->ops.context,
                                BME280_CALIBRATION_2_REGISTER,
                                driver->calibration_2,
                                BME280_CALIBRATION_2_LENGTH);
      if (!bme280_record_transport(driver, result))
      {
        return true;
      }
      if (!bme280_parse_calibration(driver->calibration_1,
                                    driver->calibration_2,
                                    &driver->calibration) ||
          !bme280_calibration_is_valid(driver->calibration_1,
                                       driver->calibration_2,
                                       &driver->calibration))
      {
        bme280_mark_error(driver, BME280_STATUS_CALIBRATION_INVALID);
        driver->state = BME280_STATE_OFFLINE;
        return true;
      }
      driver->state = BME280_STATE_CONFIGURE_HUMIDITY;
      return true;

    case BME280_STATE_CONFIGURE_HUMIDITY:
      result = driver->ops.write(driver->ops.context,
                                 BME280_CTRL_HUM_REGISTER,
                                 BME280_CTRL_HUMIDITY_X1);
      if (bme280_record_transport(driver, result))
      {
        driver->state = BME280_STATE_CONFIGURE_FILTER;
      }
      return true;

    case BME280_STATE_CONFIGURE_FILTER:
      result = driver->ops.write(driver->ops.context,
                                 BME280_CONFIG_REGISTER,
                                 BME280_CONFIG_FILTER_OFF);
      if (bme280_record_transport(driver, result))
      {
        if (driver->recovery_active)
        {
          driver->recovery_success_count =
              bme280_saturating_increment(driver->recovery_success_count);
          driver->recovery_active = false;
        }
        driver->status = BME280_STATUS_INITIALIZING;
        driver->sample.status = BME280_STATUS_INITIALIZING;
        driver->state = BME280_STATE_IDLE;
        driver->next_action_ms = now_ms;
      }
      return true;

    case BME280_STATE_IDLE:
      if (bme280_time_reached(now_ms, driver->next_action_ms))
      {
        driver->state = BME280_STATE_START_FORCED;
      }
      return true;

    case BME280_STATE_START_FORCED:
      result = driver->ops.write(driver->ops.context,
                                 BME280_CTRL_MEAS_REGISTER,
                                 BME280_CTRL_MEAS_X1_X1_FORCED);
      if (!bme280_record_transport(driver, result))
      {
        return true;
      }
      driver->state = BME280_STATE_WAIT_MEASUREMENT;
      driver->status = BME280_STATUS_NOT_READY;
      driver->sample.status = BME280_STATUS_NOT_READY;
      driver->next_action_ms = now_ms + BME280_MEASUREMENT_READY_DELAY_MS;
      driver->deadline_ms = now_ms + BME280_MEASUREMENT_DEADLINE_MS;
      return true;

    case BME280_STATE_WAIT_MEASUREMENT:
      if (!bme280_time_reached(now_ms, driver->next_action_ms))
      {
        return true;
      }
      if (bme280_time_reached(now_ms, driver->deadline_ms))
      {
        bme280_request_recovery_internal(driver,
                                         BME280_STATUS_MEASUREMENT_TIMEOUT);
        return true;
      }
      result = driver->ops.read(driver->ops.context,
                                BME280_STATUS_REGISTER,
                                &value,
                                1U);
      if (!bme280_record_transport(driver, result))
      {
        return true;
      }
      if ((value & BME280_STATUS_MEASURING_MASK) == 0U)
      {
        driver->state = BME280_STATE_READ_RAW;
      }
      else
      {
        driver->next_action_ms = driver->deadline_ms;
      }
      return true;

    case BME280_STATE_READ_RAW:
    {
      uint8_t raw_bytes[BME280_RAW_DATA_LENGTH];
      bme280_raw_sample_t raw;
      result = driver->ops.read(driver->ops.context,
                                BME280_RAW_DATA_REGISTER,
                                raw_bytes,
                                sizeof(raw_bytes));
      if (!bme280_record_transport(driver, result))
      {
        return true;
      }
      bme280_sample_t next_sample = driver->sample;
      if (!bme280_parse_raw(raw_bytes, &raw) ||
          !bme280_compensate(&driver->calibration, &raw, &next_sample))
      {
        bme280_request_recovery_internal(
            driver, BME280_STATUS_MEASUREMENT_INVALID);
        return true;
      }
      next_sample.sequence =
          bme280_saturating_increment(driver->sample.sequence);
      driver->sample = next_sample;
      driver->status = BME280_STATUS_VALID;
      driver->recovery_attempts = 0U;
      driver->state = BME280_STATE_IDLE;
      driver->next_action_ms = now_ms + driver->sample_period_ms;
      return true;
    }

    case BME280_STATE_UNINITIALIZED:
    case BME280_STATE_OFFLINE:
    default:
      return true;
  }
}

bool bme280_request_reinitialize(bme280_t *driver)
{
  if (driver == NULL)
  {
    return false;
  }
  driver->state = BME280_STATE_READ_ID;
  driver->status = BME280_STATUS_INITIALIZING;
  driver->sample.status = BME280_STATUS_INITIALIZING;
  driver->sample.valid_mask = 0U;
  driver->recovery_attempts = 0U;
  driver->recovery_active = false;
  return true;
}

bool bme280_get_sample(const bme280_t *driver, bme280_sample_t *sample)
{
  if ((driver == NULL) || (sample == NULL))
  {
    return false;
  }
  *sample = driver->sample;
  return true;
}
