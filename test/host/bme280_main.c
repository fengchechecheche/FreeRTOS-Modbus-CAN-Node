#include "bme280.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  uint8_t chip_id;
  uint8_t status_value;
  uint8_t calibration_1[BME280_CALIBRATION_1_LENGTH];
  uint8_t calibration_2[BME280_CALIBRATION_2_LENGTH];
  uint8_t raw[BME280_RAW_DATA_LENGTH];
  bme280_transport_result_t next_result;
  uint32_t call_count;
  uint8_t write_register[16];
  uint8_t write_value[16];
  size_t write_count;
} mock_bus_t;

static void put_u16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)(value & UINT16_C(0x00ff));
  data[1] = (uint8_t)(value >> 8U);
}

static void put_s16(uint8_t *data, int16_t value)
{
  put_u16(data, (uint16_t)value);
}

static void make_reference_calibration(mock_bus_t *mock)
{
  (void)memset(mock->calibration_1, 0, sizeof(mock->calibration_1));
  (void)memset(mock->calibration_2, 0, sizeof(mock->calibration_2));
  put_u16(&mock->calibration_1[0], UINT16_C(27504));
  put_s16(&mock->calibration_1[2], INT16_C(26435));
  put_s16(&mock->calibration_1[4], INT16_C(-1000));
  put_u16(&mock->calibration_1[6], UINT16_C(36477));
  put_s16(&mock->calibration_1[8], INT16_C(-10685));
  put_s16(&mock->calibration_1[10], INT16_C(3024));
  put_s16(&mock->calibration_1[12], INT16_C(2855));
  put_s16(&mock->calibration_1[14], INT16_C(140));
  put_s16(&mock->calibration_1[16], INT16_C(-7));
  put_s16(&mock->calibration_1[18], INT16_C(15500));
  put_s16(&mock->calibration_1[20], INT16_C(-14600));
  put_s16(&mock->calibration_1[22], INT16_C(6000));
  mock->calibration_1[25] = UINT8_C(75);
  put_s16(&mock->calibration_2[0], INT16_C(362));
  mock->calibration_2[2] = UINT8_C(0);
  mock->calibration_2[3] = UINT8_C(0x14);
  mock->calibration_2[4] = UINT8_C(0x2e);
  mock->calibration_2[5] = UINT8_C(0x03);
  mock->calibration_2[6] = UINT8_C(30);
}

static void make_raw(mock_bus_t *mock,
                     uint32_t pressure,
                     uint32_t temperature,
                     uint16_t humidity)
{
  mock->raw[0] = (uint8_t)(pressure >> 12U);
  mock->raw[1] = (uint8_t)(pressure >> 4U);
  mock->raw[2] = (uint8_t)((pressure & UINT32_C(0x0f)) << 4U);
  mock->raw[3] = (uint8_t)(temperature >> 12U);
  mock->raw[4] = (uint8_t)(temperature >> 4U);
  mock->raw[5] = (uint8_t)((temperature & UINT32_C(0x0f)) << 4U);
  mock->raw[6] = (uint8_t)(humidity >> 8U);
  mock->raw[7] = (uint8_t)humidity;
}

static void make_reference_bus(mock_bus_t *mock)
{
  (void)memset(mock, 0, sizeof(*mock));
  mock->chip_id = BME280_CHIP_ID;
  mock->next_result = BME280_TRANSPORT_OK;
  make_reference_calibration(mock);
  make_raw(mock, UINT32_C(415148), UINT32_C(519888), UINT16_C(35000));
}

static bme280_transport_result_t mock_take_result(mock_bus_t *mock)
{
  const bme280_transport_result_t result = mock->next_result;
  mock->next_result = BME280_TRANSPORT_OK;
  return result;
}

static bme280_transport_result_t mock_read(void *context,
                                          uint8_t register_address,
                                          uint8_t *data,
                                          size_t length)
{
  mock_bus_t *const mock = context;
  ++mock->call_count;
  const bme280_transport_result_t result = mock_take_result(mock);
  if (result != BME280_TRANSPORT_OK)
  {
    return result;
  }
  if ((register_address == BME280_CHIP_ID_REGISTER) && (length == 1U))
  {
    data[0] = mock->chip_id;
  }
  else if ((register_address == BME280_STATUS_REGISTER) && (length == 1U))
  {
    data[0] = mock->status_value;
  }
  else if ((register_address == BME280_CALIBRATION_1_REGISTER) &&
           (length == BME280_CALIBRATION_1_LENGTH))
  {
    (void)memcpy(data, mock->calibration_1, length);
  }
  else if ((register_address == BME280_CALIBRATION_2_REGISTER) &&
           (length == BME280_CALIBRATION_2_LENGTH))
  {
    (void)memcpy(data, mock->calibration_2, length);
  }
  else if ((register_address == BME280_RAW_DATA_REGISTER) &&
           (length == BME280_RAW_DATA_LENGTH))
  {
    (void)memcpy(data, mock->raw, length);
  }
  else
  {
    return BME280_TRANSPORT_INVALID_ARGUMENT;
  }
  return BME280_TRANSPORT_OK;
}

static bme280_transport_result_t mock_write(void *context,
                                           uint8_t register_address,
                                           uint8_t value)
{
  mock_bus_t *const mock = context;
  ++mock->call_count;
  const bme280_transport_result_t result = mock_take_result(mock);
  if (result != BME280_TRANSPORT_OK)
  {
    return result;
  }
  assert(mock->write_count < (sizeof(mock->write_register) /
                              sizeof(mock->write_register[0])));
  mock->write_register[mock->write_count] = register_address;
  mock->write_value[mock->write_count] = value;
  ++mock->write_count;
  return BME280_TRANSPORT_OK;
}

static bme280_ops_t make_ops(mock_bus_t *mock)
{
  const bme280_ops_t ops = {
      .context = mock,
      .read = mock_read,
      .write = mock_write,
  };
  return ops;
}

static void service_once(bme280_t *driver,
                         mock_bus_t *mock,
                         uint32_t now_ms)
{
  const uint32_t before = mock->call_count;
  assert(bme280_service(driver, now_ms));
  assert((mock->call_count - before) <= 1U);
}

static void run_until_first_sample(bme280_t *driver,
                                   mock_bus_t *mock,
                                   uint32_t start_ms)
{
  for (uint32_t step = 0U; step < 16U; ++step)
  {
    service_once(driver, mock, start_ms + step * UINT32_C(20));
    if (driver->sample.sequence == 1U)
    {
      return;
    }
  }
  assert(false);
}

static void test_calibration_and_compensation(void)
{
  mock_bus_t mock;
  make_reference_bus(&mock);
  bme280_calibration_t calibration;
  assert(bme280_parse_calibration(mock.calibration_1,
                                  mock.calibration_2,
                                  &calibration));
  assert(bme280_calibration_is_valid(mock.calibration_1,
                                     mock.calibration_2,
                                     &calibration));
  assert(calibration.dig_t1 == 27504U);
  assert(calibration.dig_t2 == 26435);
  assert(calibration.dig_t3 == -1000);
  assert(calibration.dig_p1 == 36477U);
  assert(calibration.dig_p2 == -10685);
  assert(calibration.dig_h4 == 334);
  assert(calibration.dig_h5 == 50);
  assert(calibration.dig_h6 == 30);

  bme280_raw_sample_t raw;
  assert(bme280_parse_raw(mock.raw, &raw));
  assert(raw.pressure == UINT32_C(415148));
  assert(raw.temperature == UINT32_C(519888));
  assert(raw.humidity == UINT16_C(35000));

  bme280_sample_t sample = {0};
  assert(bme280_compensate(&calibration, &raw, &sample));
  assert(sample.temperature_centi_c == 2508);
  assert(sample.pressure_pa == UINT32_C(100653));
  assert(sample.humidity_milli_pct == UINT32_C(75270));
  assert(sample.valid_mask == BME280_SAMPLE_VALID_ALL);

  raw.humidity = 0U;
  assert(bme280_compensate(&calibration, &raw, &sample));
  assert(sample.humidity_milli_pct == 0U);
  raw.humidity = UINT16_MAX;
  assert(bme280_compensate(&calibration, &raw, &sample));
  assert(sample.humidity_milli_pct <= UINT32_C(100000));

  raw.temperature = UINT32_C(0x80000);
  assert(!bme280_compensate(&calibration, &raw, &sample));
}

static void test_additional_reference_vectors(void)
{
  mock_bus_t mock;
  make_reference_bus(&mock);
  bme280_calibration_t calibration;
  assert(bme280_parse_calibration(mock.calibration_1,
                                  mock.calibration_2,
                                  &calibration));
  const bme280_raw_sample_t raw[] = {
      {UINT32_C(480000), UINT32_C(400000), UINT16_C(20000)},
      {UINT32_C(560000), UINT32_C(500000), UINT16_C(60000)},
  };
  const int32_t expected_temperature[] = {INT32_C(1257), INT32_C(3763)};
  const uint32_t expected_pressure[] = {UINT32_C(101295), UINT32_C(87679)};
  const uint32_t expected_humidity[] = {UINT32_C(0), UINT32_C(100000)};
  for (size_t index = 0U; index < (sizeof(raw) / sizeof(raw[0])); ++index)
  {
    bme280_sample_t sample = {0};
    assert(bme280_compensate(&calibration, &raw[index], &sample));
    assert(sample.temperature_centi_c == expected_temperature[index]);
    assert(sample.pressure_pa == expected_pressure[index]);
    assert(sample.humidity_milli_pct == expected_humidity[index]);
  }
}

static void test_calibration_validation_and_sign_extension(void)
{
  uint8_t first[BME280_CALIBRATION_1_LENGTH] = {0};
  uint8_t second[BME280_CALIBRATION_2_LENGTH] = {0};
  bme280_calibration_t calibration;
  assert(bme280_parse_calibration(first, second, &calibration));
  assert(!bme280_calibration_is_valid(first, second, &calibration));

  (void)memset(first, UINT8_MAX, sizeof(first));
  (void)memset(second, UINT8_MAX, sizeof(second));
  assert(bme280_parse_calibration(first, second, &calibration));
  assert(!bme280_calibration_is_valid(first, second, &calibration));

  mock_bus_t mock;
  make_reference_bus(&mock);
  mock.calibration_2[3] = UINT8_C(0xf0);
  mock.calibration_2[4] = UINT8_C(0x8f);
  mock.calibration_2[5] = UINT8_C(0xff);
  assert(bme280_parse_calibration(mock.calibration_1,
                                  mock.calibration_2,
                                  &calibration));
  assert(calibration.dig_h4 == -241);
  assert(calibration.dig_h5 == -8);

  make_reference_bus(&mock);
  mock.calibration_1[6] = 0U;
  mock.calibration_1[7] = 0U;
  assert(bme280_parse_calibration(mock.calibration_1,
                                  mock.calibration_2,
                                  &calibration));
  assert(!bme280_calibration_is_valid(mock.calibration_1,
                                      mock.calibration_2,
                                      &calibration));
}

static void test_successful_state_machine(void)
{
  mock_bus_t mock;
  make_reference_bus(&mock);
  const bme280_ops_t ops = make_ops(&mock);
  bme280_t driver;
  assert(bme280_initialize(
      &driver, &ops, BME280_DEFAULT_SAMPLE_PERIOD_MS));
  run_until_first_sample(&driver, &mock, 0U);

  assert(driver.status == BME280_STATUS_VALID);
  assert(driver.state == BME280_STATE_IDLE);
  assert(driver.sample.sequence == 1U);
  assert(driver.sample.temperature_centi_c == 2508);
  assert(driver.sample.pressure_pa == UINT32_C(100653));
  assert(mock.write_count == 4U);
  assert(mock.write_register[0] == BME280_RESET_REGISTER);
  assert(mock.write_value[0] == BME280_RESET_COMMAND);
  assert(mock.write_register[1] == BME280_CTRL_HUM_REGISTER);
  assert(mock.write_value[1] == BME280_CTRL_HUMIDITY_X1);
  assert(mock.write_register[2] == BME280_CONFIG_REGISTER);
  assert(mock.write_value[2] == BME280_CONFIG_FILTER_OFF);
  assert(mock.write_register[3] == BME280_CTRL_MEAS_REGISTER);
  assert(mock.write_value[3] == BME280_CTRL_MEAS_X1_X1_FORCED);

  bme280_sample_t snapshot;
  assert(bme280_get_sample(&driver, &snapshot));
  assert(snapshot.sequence == 1U);
}

static void test_identity_and_invalid_calibration(void)
{
  mock_bus_t mock;
  make_reference_bus(&mock);
  bme280_t driver;
  bme280_ops_t ops = make_ops(&mock);

  mock.chip_id = BME280_BMP280_CHIP_ID;
  assert(bme280_initialize(&driver, &ops, BME280_DEFAULT_SAMPLE_PERIOD_MS));
  service_once(&driver, &mock, 0U);
  assert(driver.state == BME280_STATE_OFFLINE);
  assert(driver.status == BME280_STATUS_UNSUPPORTED_BMP280);

  make_reference_bus(&mock);
  mock.chip_id = UINT8_C(0x61);
  ops = make_ops(&mock);
  assert(bme280_initialize(&driver, &ops, BME280_DEFAULT_SAMPLE_PERIOD_MS));
  service_once(&driver, &mock, 0U);
  assert(driver.status == BME280_STATUS_WRONG_ID);

  make_reference_bus(&mock);
  (void)memset(mock.calibration_1, 0, sizeof(mock.calibration_1));
  (void)memset(mock.calibration_2, 0, sizeof(mock.calibration_2));
  ops = make_ops(&mock);
  assert(bme280_initialize(&driver, &ops, BME280_DEFAULT_SAMPLE_PERIOD_MS));
  for (uint32_t step = 0U; step < 8U; ++step)
  {
    service_once(&driver, &mock, step * UINT32_C(20));
  }
  assert(driver.state == BME280_STATE_OFFLINE);
  assert(driver.status == BME280_STATUS_CALIBRATION_INVALID);
}

static void test_bounded_recovery(void)
{
  mock_bus_t mock;
  make_reference_bus(&mock);
  bme280_ops_t ops = make_ops(&mock);
  bme280_t driver;
  assert(bme280_initialize(&driver, &ops, BME280_DEFAULT_SAMPLE_PERIOD_MS));

  mock.next_result = BME280_TRANSPORT_TIMEOUT;
  service_once(&driver, &mock, 0U);
  assert(driver.state == BME280_STATE_SOFT_RESET);
  assert(driver.status == BME280_STATUS_RECOVERY_REQUIRED);
  assert(driver.last_transport_result == BME280_TRANSPORT_TIMEOUT);
  assert(driver.recovery_request_count == 1U);

  mock.next_result = BME280_TRANSPORT_IO_ERROR;
  service_once(&driver, &mock, UINT32_C(20));
  assert(driver.state == BME280_STATE_OFFLINE);
  assert(driver.recovery_request_count == 1U);
  assert(driver.error_count == 2U);

  assert(bme280_request_reinitialize(&driver));
  assert(driver.state == BME280_STATE_READ_ID);
  assert(driver.sample.valid_mask == 0U);

  make_reference_bus(&mock);
  ops = make_ops(&mock);
  assert(bme280_initialize(&driver, &ops, BME280_DEFAULT_SAMPLE_PERIOD_MS));
  mock.next_result = BME280_TRANSPORT_BUSY;
  service_once(&driver, &mock, 0U);
  assert(driver.status == BME280_STATUS_RECOVERY_REQUIRED);
  assert(driver.last_transport_result == BME280_TRANSPORT_BUSY);
  run_until_first_sample(&driver, &mock, UINT32_C(20));
  assert(driver.recovery_success_count == 1U);
  assert(driver.recovery_request_count == 1U);
}

static void test_timeouts_and_wrap(void)
{
  mock_bus_t mock;
  make_reference_bus(&mock);
  bme280_ops_t ops = make_ops(&mock);
  bme280_t driver;
  assert(bme280_initialize(&driver, &ops, BME280_DEFAULT_SAMPLE_PERIOD_MS));
  service_once(&driver, &mock, UINT32_MAX - UINT32_C(30));
  service_once(&driver, &mock, UINT32_MAX - UINT32_C(10));
  service_once(&driver, &mock, UINT32_C(10));
  assert(driver.state == BME280_STATE_READ_CALIBRATION_1);

  make_reference_bus(&mock);
  mock.status_value = BME280_STATUS_IM_UPDATE_MASK;
  ops = make_ops(&mock);
  assert(bme280_initialize(&driver, &ops, BME280_DEFAULT_SAMPLE_PERIOD_MS));
  for (uint32_t step = 0U; step <= 6U; ++step)
  {
    service_once(&driver, &mock, step * UINT32_C(20));
  }
  assert(driver.status == BME280_STATUS_RECOVERY_REQUIRED);
  assert(driver.recovery_request_count == 1U);

  make_reference_bus(&mock);
  ops = make_ops(&mock);
  assert(bme280_initialize(&driver, &ops, BME280_DEFAULT_SAMPLE_PERIOD_MS));
  for (uint32_t step = 0U; step <= 8U; ++step)
  {
    service_once(&driver, &mock, step * UINT32_C(20));
  }
  assert(driver.state == BME280_STATE_WAIT_MEASUREMENT);
  mock.status_value = BME280_STATUS_MEASURING_MASK;
  service_once(&driver, &mock, UINT32_C(180));
  service_once(&driver, &mock, UINT32_C(200));
  assert(driver.status == BME280_STATUS_RECOVERY_REQUIRED);
  assert(driver.recovery_request_count == 1U);

  assert(!bme280_initialize(NULL, &ops, BME280_DEFAULT_SAMPLE_PERIOD_MS));
  assert(!bme280_initialize(&driver, NULL, BME280_DEFAULT_SAMPLE_PERIOD_MS));
  assert(!bme280_initialize(&driver, &ops, 0U));
  assert(!bme280_get_sample(NULL, &driver.sample));
  assert(!bme280_get_sample(&driver, NULL));
}

int main(void)
{
  test_calibration_and_compensation();
  test_additional_reference_vectors();
  test_calibration_validation_and_sign_extension();
  test_successful_state_machine();
  test_identity_and_invalid_calibration();
  test_bounded_recovery();
  test_timeouts_and_wrap();
  puts("P5 host bme280: PASS");
  return 0;
}
