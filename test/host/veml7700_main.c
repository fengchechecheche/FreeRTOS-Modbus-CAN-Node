#include "veml7700.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  uint16_t config_word;
  uint16_t raw_als;
  veml7700_transport_result_t next_result;
  bool recovery_available;
  bool ignore_config_write;
  uint32_t call_count;
  uint8_t register_log[128];
  uint8_t data_log[128][VEML7700_WORD_LENGTH];
  size_t length_log[128];
  bool write_log[128];
  size_t log_count;
} mock_bus_t;

static void make_mock(mock_bus_t *mock, uint16_t raw_als)
{
  (void)memset(mock, 0, sizeof(*mock));
  mock->config_word = UINT16_C(0x0001);
  mock->raw_als = raw_als;
  mock->next_result = VEML7700_TRANSPORT_OK;
}

static void log_call(mock_bus_t *mock,
                     bool write,
                     uint8_t register_address,
                     const uint8_t *data,
                     size_t length)
{
  assert(mock->log_count <
         (sizeof(mock->register_log) / sizeof(mock->register_log[0])));
  const size_t index = mock->log_count;
  mock->write_log[index] = write;
  mock->register_log[index] = register_address;
  mock->length_log[index] = length;
  if ((data != NULL) && (length == VEML7700_WORD_LENGTH))
  {
    (void)memcpy(mock->data_log[index], data, length);
  }
  ++mock->log_count;
}

static veml7700_transport_result_t take_result(mock_bus_t *mock)
{
  const veml7700_transport_result_t result = mock->next_result;
  mock->next_result = VEML7700_TRANSPORT_OK;
  return result;
}

static veml7700_transport_result_t mock_read(void *context,
                                             uint8_t register_address,
                                             uint8_t *data,
                                             size_t length)
{
  mock_bus_t *const mock = context;
  ++mock->call_count;
  log_call(mock, false, register_address, NULL, length);
  const veml7700_transport_result_t result = take_result(mock);
  if (result != VEML7700_TRANSPORT_OK)
  {
    return result;
  }
  if ((data == NULL) || (length != VEML7700_WORD_LENGTH))
  {
    return VEML7700_TRANSPORT_INVALID_ARGUMENT;
  }
  if (register_address == VEML7700_CONFIG_REGISTER)
  {
    (void)veml7700_encode_word(mock->config_word, data);
  }
  else if (register_address == VEML7700_ALS_REGISTER)
  {
    (void)veml7700_encode_word(mock->raw_als, data);
  }
  else
  {
    return VEML7700_TRANSPORT_INVALID_ARGUMENT;
  }
  return VEML7700_TRANSPORT_OK;
}

static veml7700_transport_result_t mock_write(void *context,
                                              uint8_t register_address,
                                              const uint8_t *data,
                                              size_t length)
{
  mock_bus_t *const mock = context;
  ++mock->call_count;
  log_call(mock, true, register_address, data, length);
  const veml7700_transport_result_t result = take_result(mock);
  if (result != VEML7700_TRANSPORT_OK)
  {
    return result;
  }
  if ((register_address != VEML7700_CONFIG_REGISTER) || (data == NULL) ||
      (length != VEML7700_WORD_LENGTH))
  {
    return VEML7700_TRANSPORT_INVALID_ARGUMENT;
  }
  if (!mock->ignore_config_write)
  {
    mock->config_word = veml7700_decode_word(data);
  }
  return VEML7700_TRANSPORT_OK;
}

static bool mock_request_recovery(void *context)
{
  const mock_bus_t *const mock = context;
  return mock->recovery_available;
}

static veml7700_ops_t make_ops(mock_bus_t *mock)
{
  const veml7700_ops_t ops = {
      .context = mock,
      .read = mock_read,
      .write = mock_write,
      .request_recovery = mock_request_recovery,
  };
  return ops;
}

static void service_once(veml7700_t *driver,
                         mock_bus_t *mock,
                         uint32_t now_ms)
{
  const uint32_t before = mock->call_count;
  assert(veml7700_service(driver, now_ms));
  assert((mock->call_count - before) <= 1U);
}

static void run_until_sample(veml7700_t *driver,
                             mock_bus_t *mock,
                             uint32_t start_ms,
                             uint32_t maximum_steps)
{
  for (uint32_t step = 0U; step < maximum_steps; ++step)
  {
    service_once(driver, mock, start_ms + step * UINT32_C(20));
    if (driver->sample.sequence != 0U)
    {
      return;
    }
  }
  assert(false);
}

static void test_word_range_and_conversion(void)
{
  uint8_t data[VEML7700_WORD_LENGTH];
  assert(veml7700_encode_word(UINT16_C(0x15cd), data));
  assert(data[0] == UINT8_C(0xcd));
  assert(data[1] == UINT8_C(0x15));
  assert(veml7700_decode_word(data) == UINT16_C(0x15cd));
  assert(!veml7700_encode_word(0U, NULL));
  assert(veml7700_decode_word(NULL) == 0U);

  static const uint16_t expected_config[VEML7700_RANGE_LEVEL_COUNT] = {
      UINT16_C(0x1300), UINT16_C(0x1200), UINT16_C(0x1000),
      UINT16_C(0x1800), UINT16_C(0x0000), UINT16_C(0x0800),
      UINT16_C(0x0840), UINT16_C(0x0880), UINT16_C(0x08c0),
  };
  static const uint16_t expected_integration[VEML7700_RANGE_LEVEL_COUNT] = {
      UINT16_C(25), UINT16_C(50), UINT16_C(100),
      UINT16_C(100), UINT16_C(100), UINT16_C(100),
      UINT16_C(200), UINT16_C(400), UINT16_C(800),
  };
  for (uint8_t level = 0U; level < VEML7700_RANGE_LEVEL_COUNT; ++level)
  {
    veml7700_range_config_t range;
    assert(veml7700_get_range_config(level, &range));
    assert(range.config_word == expected_config[level]);
    assert(range.integration_ms == expected_integration[level]);
  }
  veml7700_range_config_t range;
  assert(!veml7700_get_range_config(VEML7700_RANGE_LEVEL_COUNT, &range));
  assert(!veml7700_get_range_config(0U, NULL));

  uint32_t millilux = UINT32_MAX;
  assert(veml7700_convert_millilux(0U, 2U, &millilux));
  assert(millilux == 0U);
  assert(veml7700_convert_millilux(UINT16_C(100), 2U, &millilux));
  assert(millilux == UINT32_C(53760));
  assert(veml7700_convert_millilux(UINT16_C(5581), 2U, &millilux));
  assert(millilux == UINT32_C(3000346));
  assert(veml7700_convert_millilux(UINT16_C(10000), 2U, &millilux));
  assert(millilux == UINT32_C(5376000));
  assert(veml7700_convert_millilux(UINT16_MAX, 0U, &millilux));
  assert(millilux == UINT32_C(140926464));
  assert(!veml7700_convert_millilux(1U, VEML7700_RANGE_LEVEL_COUNT,
                                    &millilux));
  assert(!veml7700_convert_millilux(1U, 0U, NULL));
}

static void test_stable_state_machine(void)
{
  mock_bus_t mock;
  make_mock(&mock, UINT16_C(1000));
  const veml7700_ops_t ops = make_ops(&mock);
  veml7700_t driver;
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  run_until_sample(&driver, &mock, 0U, 20U);

  assert(driver.state == VEML7700_STATE_IDLE);
  assert(driver.status == VEML7700_STATUS_VALID);
  assert(driver.sample.sequence == 1U);
  assert(driver.sample.raw_als == UINT16_C(1000));
  assert(driver.sample.illuminance_millilux == UINT32_C(537600));
  assert(driver.sample.range_level == VEML7700_DEFAULT_RANGE_LEVEL);
  assert(driver.sample.integration_ms == UINT16_C(100));
  assert(driver.sample.gain == VEML7700_GAIN_ONE_EIGHTH);
  assert(driver.sample.quality_flags == VEML7700_QUALITY_VALID);

  assert(mock.log_count == 4U);
  assert(!mock.write_log[0]);
  assert(mock.register_log[0] == VEML7700_CONFIG_REGISTER);
  assert(mock.write_log[1]);
  assert(mock.register_log[1] == VEML7700_CONFIG_REGISTER);
  assert(mock.data_log[1][0] == UINT8_C(0x00));
  assert(mock.data_log[1][1] == UINT8_C(0x10));
  assert(!mock.write_log[2]);
  assert(mock.register_log[2] == VEML7700_CONFIG_REGISTER);
  assert(!mock.write_log[3]);
  assert(mock.register_log[3] == VEML7700_ALS_REGISTER);
  for (size_t index = 0U; index < mock.log_count; ++index)
  {
    assert(mock.length_log[index] == VEML7700_WORD_LENGTH);
  }

  veml7700_sample_t snapshot;
  assert(veml7700_get_sample(&driver, &snapshot));
  assert(snapshot.sequence == 1U);
}

static void test_low_and_high_range_limits(void)
{
  mock_bus_t mock;
  make_mock(&mock, 0U);
  veml7700_ops_t ops = make_ops(&mock);
  veml7700_t driver;
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  run_until_sample(&driver, &mock, 0U, 400U);
  assert(driver.sample.range_level == 8U);
  assert(driver.range_change_count == 6U);
  assert(driver.sample.raw_als == 0U);
  assert(driver.sample.status == VEML7700_STATUS_RANGE_LIMITED);
  assert((driver.sample.quality_flags & VEML7700_QUALITY_VALID) != 0U);
  assert((driver.sample.quality_flags & VEML7700_QUALITY_LOW_COUNT) != 0U);
  assert((driver.sample.quality_flags &
          VEML7700_QUALITY_RANGE_LIMITED) != 0U);

  make_mock(&mock, UINT16_MAX);
  ops = make_ops(&mock);
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  run_until_sample(&driver, &mock, 0U, 100U);
  assert(driver.sample.range_level == 0U);
  assert(driver.range_change_count == 2U);
  assert(driver.sample.status == VEML7700_STATUS_SATURATED);
  assert((driver.sample.quality_flags & VEML7700_QUALITY_SATURATED) != 0U);
  assert((driver.sample.quality_flags &
          VEML7700_QUALITY_HIGH_LUX_UNCORRECTED) != 0U);
  assert(driver.sample.illuminance_millilux == UINT32_C(140926464));
}

static void test_threshold_hysteresis(void)
{
  mock_bus_t mock;
  make_mock(&mock, UINT16_C(101));
  veml7700_ops_t ops = make_ops(&mock);
  veml7700_t driver;
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  run_until_sample(&driver, &mock, 0U, 20U);
  assert(driver.sample.range_level == 2U);
  assert(driver.range_change_count == 0U);

  make_mock(&mock, UINT16_C(100));
  ops = make_ops(&mock);
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  for (uint32_t step = 0U; step < 20U; ++step)
  {
    service_once(&driver, &mock, step * UINT32_C(20));
    if (driver.range_change_count == 1U)
    {
      break;
    }
  }
  assert(driver.range_level == 3U);

  make_mock(&mock, UINT16_C(9999));
  ops = make_ops(&mock);
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  run_until_sample(&driver, &mock, 0U, 20U);
  assert(driver.sample.range_level == 2U);

  make_mock(&mock, UINT16_C(10000));
  ops = make_ops(&mock);
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  for (uint32_t step = 0U; step < 20U; ++step)
  {
    service_once(&driver, &mock, step * UINT32_C(20));
    if (driver.range_change_count == 1U)
    {
      break;
    }
  }
  assert(driver.range_level == 1U);
}

static void test_transport_and_recovery(void)
{
  mock_bus_t mock;
  make_mock(&mock, UINT16_C(1000));
  veml7700_ops_t ops = make_ops(&mock);
  veml7700_t driver;
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  mock.next_result = VEML7700_TRANSPORT_NOT_PRESENT;
  service_once(&driver, &mock, 0U);
  assert(driver.state == VEML7700_STATE_OFFLINE);
  assert(driver.status == VEML7700_STATUS_RECOVERY_UNAVAILABLE);
  assert(driver.last_error_status == VEML7700_STATUS_NOT_PRESENT);
  assert(driver.last_transport_result == VEML7700_TRANSPORT_NOT_PRESENT);

  make_mock(&mock, UINT16_C(1000));
  mock.recovery_available = true;
  ops = make_ops(&mock);
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  mock.next_result = VEML7700_TRANSPORT_BUSY;
  service_once(&driver, &mock, 0U);
  assert(driver.state == VEML7700_STATE_READ_CONFIG);
  assert(driver.status == VEML7700_STATUS_RECOVERY_REQUIRED);
  run_until_sample(&driver, &mock, UINT32_C(20), 20U);
  assert(driver.recovery_request_count == 1U);
  assert(driver.recovery_success_count == 1U);
  assert(driver.sample.sequence == 1U);

  make_mock(&mock, UINT16_C(1000));
  mock.recovery_available = true;
  ops = make_ops(&mock);
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  mock.next_result = VEML7700_TRANSPORT_TIMEOUT;
  service_once(&driver, &mock, 0U);
  mock.next_result = VEML7700_TRANSPORT_IO_ERROR;
  service_once(&driver, &mock, UINT32_C(20));
  assert(driver.state == VEML7700_STATE_OFFLINE);
  assert(driver.status == VEML7700_STATUS_OFFLINE);
  assert(driver.recovery_request_count == 1U);
  assert(driver.error_count == 2U);

  make_mock(&mock, UINT16_C(1000));
  mock.ignore_config_write = true;
  ops = make_ops(&mock);
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  service_once(&driver, &mock, 0U);
  service_once(&driver, &mock, UINT32_C(20));
  service_once(&driver, &mock, UINT32_C(40));
  assert(driver.state == VEML7700_STATE_OFFLINE);
  assert(driver.last_error_status ==
         VEML7700_STATUS_CONFIGURATION_MISMATCH);
}

static void test_wait_wrap_and_arguments(void)
{
  mock_bus_t mock;
  make_mock(&mock, UINT16_C(1000));
  const veml7700_ops_t ops = make_ops(&mock);
  veml7700_t driver;
  assert(veml7700_initialize(
      &driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  service_once(&driver, &mock, UINT32_MAX - UINT32_C(90));
  service_once(&driver, &mock, UINT32_MAX - UINT32_C(70));
  service_once(&driver, &mock, UINT32_MAX - UINT32_C(50));
  const uint32_t calls_after_verify = mock.call_count;
  service_once(&driver, &mock, UINT32_MAX - UINT32_C(10));
  service_once(&driver, &mock, UINT32_C(50));
  assert(mock.call_count == calls_after_verify);
  service_once(&driver, &mock, UINT32_C(100));
  service_once(&driver, &mock, UINT32_C(120));
  assert(driver.sample.sequence == 1U);

  assert(!veml7700_initialize(NULL, &ops,
                              VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  assert(!veml7700_initialize(&driver, NULL,
                              VEML7700_DEFAULT_SAMPLE_PERIOD_MS));
  assert(!veml7700_initialize(&driver, &ops, 0U));
  assert(!veml7700_service(NULL, 0U));
  assert(!veml7700_get_sample(NULL, &driver.sample));
  assert(!veml7700_get_sample(&driver, NULL));
  assert(veml7700_request_reinitialize(&driver));
  assert(driver.state == VEML7700_STATE_READ_CONFIG);
  assert(driver.sample.quality_flags == 0U);
  assert(!veml7700_request_reinitialize(NULL));
}

int main(void)
{
  test_word_range_and_conversion();
  test_stable_state_machine();
  test_low_and_high_range_limits();
  test_threshold_hysteresis();
  test_transport_and_recovery();
  test_wait_wrap_and_arguments();
  puts("P5 host veml7700: PASS");
  return 0;
}
