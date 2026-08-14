#include "adxl345.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)          \
  do                              \
  {                               \
    if (!(condition))             \
    {                             \
      return EXIT_FAILURE;        \
    }                             \
  } while (false)

typedef struct
{
  uint8_t registers[256];
  uint8_t sample_data[ADXL345_DATA_LENGTH];
  adxl345_transport_result_t next_result;
  uint32_t transaction_count;
  uint32_t data_read_count;
  uint8_t last_register;
  size_t last_length;
} mock_bus_t;

static adxl345_transport_result_t mock_result(mock_bus_t *bus)
{
  const adxl345_transport_result_t result = bus->next_result;
  bus->next_result = ADXL345_TRANSPORT_OK;
  return result;
}

static adxl345_transport_result_t mock_read(void *context,
                                            uint8_t register_address,
                                            uint8_t *data,
                                            size_t length)
{
  mock_bus_t *bus = context;
  ++bus->transaction_count;
  bus->last_register = register_address;
  bus->last_length = length;
  const adxl345_transport_result_t result = mock_result(bus);
  if (result != ADXL345_TRANSPORT_OK)
  {
    return result;
  }
  if ((register_address == ADXL345_DATAX0_REGISTER) &&
      (length == ADXL345_DATA_LENGTH))
  {
    (void)memcpy(data, bus->sample_data, length);
    ++bus->data_read_count;
  }
  else
  {
    for (size_t index = 0U; index < length; ++index)
    {
      data[index] = bus->registers[(uint8_t)(register_address + index)];
    }
  }
  return ADXL345_TRANSPORT_OK;
}

static adxl345_transport_result_t mock_write(void *context,
                                             uint8_t register_address,
                                             uint8_t value)
{
  mock_bus_t *bus = context;
  ++bus->transaction_count;
  bus->last_register = register_address;
  bus->last_length = 1U;
  const adxl345_transport_result_t result = mock_result(bus);
  if (result == ADXL345_TRANSPORT_OK)
  {
    bus->registers[register_address] = value;
  }
  return result;
}

static void mock_initialize(mock_bus_t *bus)
{
  (void)memset(bus, 0, sizeof(*bus));
  bus->registers[ADXL345_DEVID_REGISTER] = ADXL345_DEVID_VALUE;
  bus->next_result = ADXL345_TRANSPORT_OK;
}

static bool initialize_driver(adxl345_t *driver,
                              mock_bus_t *bus,
                              const adxl345_config_t *config,
                              uint32_t final_now_ms)
{
  const adxl345_ops_t ops = {
      .context = bus,
      .read = mock_read,
      .write = mock_write,
  };
  if (!adxl345_initialize(driver, &ops, config))
  {
    return false;
  }

  uint32_t guard = 0U;
  while ((driver->state != ADXL345_STATE_WAIT_DATA_READY) &&
         (guard < 20U))
  {
    const uint32_t before = bus->transaction_count;
    const uint32_t now =
        driver->state == ADXL345_STATE_ENABLE_DATA_READY ? final_now_ms
                                                         : guard;
    if (!adxl345_service(driver, now, 0U) ||
        ((bus->transaction_count - before) > 1U))
    {
      return false;
    }
    ++guard;
  }
  return driver->state == ADXL345_STATE_WAIT_DATA_READY;
}

static void encode_axis(int16_t value, uint8_t *data)
{
  const uint16_t raw = (uint16_t)value;
  data[0] = (uint8_t)(raw & UINT16_C(0x00ff));
  data[1] = (uint8_t)(raw >> 8U);
}

static void set_sample(mock_bus_t *bus, int16_t x, int16_t y, int16_t z)
{
  encode_axis(x, &bus->sample_data[0]);
  encode_axis(y, &bus->sample_data[2]);
  encode_axis(z, &bus->sample_data[4]);
}

static adxl345_sample_t feature_sample(int32_t x,
                                       int32_t y,
                                       int32_t z,
                                       uint32_t quality,
                                       uint32_t dropped)
{
  adxl345_sample_t sample;
  (void)memset(&sample, 0, sizeof(sample));
  sample.status = ADXL345_STATUS_VALID;
  sample.acceleration_millig[0] = x;
  sample.acceleration_millig[1] = y;
  sample.acceleration_millig[2] = z;
  sample.quality_flags = ADXL345_QUALITY_VALID | quality;
  sample.dropped_sample_lower_bound = dropped;
  return sample;
}

static int test_parse_and_scale(void)
{
  const uint8_t data[ADXL345_DATA_LENGTH] = {
      0x00U, 0x00U, 0xffU, 0xffU, 0x00U, 0x01U};
  int16_t raw[ADXL345_AXIS_COUNT];
  CHECK(adxl345_parse_xyz(data, raw));
  CHECK(raw[0] == 0);
  CHECK(raw[1] == -1);
  CHECK(raw[2] == 256);

  const uint8_t boundaries[ADXL345_DATA_LENGTH] = {
      0x00U, 0x80U, 0xffU, 0x7fU, 0x00U, 0xffU};
  CHECK(adxl345_parse_xyz(boundaries, raw));
  CHECK(raw[0] == INT16_MIN);
  CHECK(raw[1] == INT16_MAX);
  CHECK(raw[2] == -256);
  CHECK(adxl345_counts_to_millig(0) == 0);
  CHECK(adxl345_counts_to_millig(1) == 4);
  CHECK(adxl345_counts_to_millig(-1) == -4);
  CHECK(adxl345_counts_to_millig(256) == 1000);
  CHECK(adxl345_counts_to_millig(-256) == -1000);
  return EXIT_SUCCESS;
}

static int test_configuration_sequence_and_bias(void)
{
  mock_bus_t bus;
  adxl345_t driver;
  const adxl345_config_t config = {{1, -1, 256}, true};
  mock_initialize(&bus);
  CHECK(initialize_driver(&driver, &bus, &config, 20U));
  CHECK(bus.transaction_count == 13U);
  CHECK(bus.registers[ADXL345_FIFO_CTL_REGISTER] == ADXL345_FIFO_BYPASS);
  CHECK(bus.registers[ADXL345_DATA_FORMAT_REGISTER] ==
        ADXL345_DATA_FORMAT_FULL_RES_4G);
  CHECK(bus.registers[ADXL345_BW_RATE_REGISTER] == ADXL345_BW_RATE_100_HZ);
  CHECK(bus.registers[ADXL345_INT_MAP_REGISTER] ==
        ADXL345_INT_MAP_DATA_READY_INT1);
  CHECK(bus.registers[ADXL345_POWER_CTL_REGISTER] ==
        ADXL345_POWER_CTL_MEASURE);
  CHECK(bus.registers[ADXL345_INT_ENABLE_REGISTER] ==
        ADXL345_INT_DATA_READY);

  set_sample(&bus, 257, -257, 512);
  const uint32_t before = bus.transaction_count;
  CHECK(adxl345_service(&driver, 30U, 1U));
  CHECK((bus.transaction_count - before) == 1U);
  CHECK(bus.last_register == ADXL345_DATAX0_REGISTER);
  CHECK(bus.last_length == ADXL345_DATA_LENGTH);
  CHECK(driver.sample.corrected_counts[0] == 256);
  CHECK(driver.sample.corrected_counts[1] == -256);
  CHECK(driver.sample.corrected_counts[2] == 256);
  CHECK(driver.sample.acceleration_millig[0] == 1000);
  CHECK(driver.sample.acceleration_millig[1] == -1000);
  CHECK(driver.sample.acceleration_millig[2] == 1000);
  CHECK((driver.sample.quality_flags &
         ADXL345_QUALITY_BIAS_UNCALIBRATED) == 0U);
  return EXIT_SUCCESS;
}

static int test_feature_windows(void)
{
  adxl345_feature_window_t window;
  adxl345_feature_t feature;
  (void)memset(&feature, 0, sizeof(feature));
  adxl345_feature_window_initialize(&window);

  for (uint32_t index = 0U; index < 100U; ++index)
  {
    const adxl345_sample_t sample = feature_sample(0, 0, 1000, 0U, 0U);
    const bool ready = adxl345_feature_window_push(&window, &sample, &feature);
    CHECK(ready == (index == 99U));
  }
  CHECK(feature.sequence == 1U);
  CHECK(feature.rms_millig[0] == 0U);
  CHECK(feature.rms_millig[1] == 0U);
  CHECK(feature.rms_millig[2] == 0U);
  CHECK(feature.resultant_rms_millig == 0U);

  for (uint32_t index = 0U; index < 100U; ++index)
  {
    const int32_t sign = (index & 1U) == 0U ? 1000 : -1000;
    const adxl345_sample_t sample = feature_sample(sign, sign, 0, 0U, 0U);
    CHECK(adxl345_feature_window_push(&window, &sample, &feature) ==
          (index == 99U));
  }
  CHECK(feature.sequence == 2U);
  CHECK(feature.mean_millig[0] == 0);
  CHECK(feature.rms_millig[0] == 1000U);
  CHECK(feature.peak_abs_millig[0] == 1000U);
  CHECK(feature.resultant_rms_millig == 1414U);

  for (uint32_t index = 0U; index < 100U; ++index)
  {
    const int32_t value = index < 50U ? 0 : 1000;
    const adxl345_sample_t sample = feature_sample(
        value,
        0,
        0,
        index == 50U ? ADXL345_QUALITY_GAP : 0U,
        index == 50U ? 2U : 0U);
    CHECK(adxl345_feature_window_push(&window, &sample, &feature) ==
          (index == 99U));
  }
  CHECK(feature.sequence == 3U);
  CHECK(feature.mean_millig[0] == 500);
  CHECK(feature.rms_millig[0] == 500U);
  CHECK(feature.peak_abs_millig[0] == 500U);
  CHECK((feature.quality_flags & ADXL345_QUALITY_GAP) != 0U);
  CHECK(feature.dropped_sample_lower_bound == 2U);

  const adxl345_sample_t extra = feature_sample(1, 2, 3, 0U, 0U);
  CHECK(!adxl345_feature_window_push(&window, &extra, &feature));
  CHECK(window.count == 1U);
  CHECK(feature.sequence == 3U);
  return EXIT_SUCCESS;
}

static int test_event_admission_and_drop_lower_bound(void)
{
  mock_bus_t bus;
  adxl345_t driver;
  const adxl345_config_t config = {{0, 0, 0}, false};
  mock_initialize(&bus);
  CHECK(initialize_driver(&driver, &bus, &config, 0U));
  set_sample(&bus, 0, 0, 256);

  const uint32_t before = bus.transaction_count;
  CHECK(adxl345_service(&driver, 10U, 3U));
  CHECK((bus.transaction_count - before) == 1U);
  CHECK(bus.data_read_count == 1U);
  CHECK(driver.irq_event_count == 3U);
  CHECK(driver.dropped_sample_lower_bound == 2U);
  CHECK(driver.sample.dropped_sample_lower_bound == 2U);
  CHECK((driver.sample.quality_flags & ADXL345_QUALITY_DROPPED) != 0U);
  CHECK((driver.sample.quality_flags & ADXL345_QUALITY_GAP) != 0U);
  CHECK((driver.sample.quality_flags &
         ADXL345_QUALITY_BIAS_UNCALIBRATED) != 0U);

  const uint32_t after_sample = bus.transaction_count;
  CHECK(adxl345_service(&driver, 11U, 0U));
  CHECK(bus.transaction_count == after_sample);
  return EXIT_SUCCESS;
}

static int test_identity_configuration_and_transport_failures(void)
{
  mock_bus_t bus;
  adxl345_t driver;
  const adxl345_config_t config = {{0, 0, 0}, false};
  const adxl345_ops_t ops = {&bus, mock_read, mock_write};

  mock_initialize(&bus);
  bus.registers[ADXL345_DEVID_REGISTER] = 0U;
  CHECK(adxl345_initialize(&driver, &ops, &config));
  CHECK(adxl345_service(&driver, 0U, 0U));
  CHECK(driver.status == ADXL345_STATUS_WRONG_ID);
  CHECK(driver.state == ADXL345_STATE_OFFLINE);

  mock_initialize(&bus);
  CHECK(adxl345_initialize(&driver, &ops, &config));
  bus.next_result = ADXL345_TRANSPORT_BUSY;
  CHECK(adxl345_service(&driver, 0U, 0U));
  CHECK(driver.last_error_status == ADXL345_STATUS_TRANSPORT_BUSY);
  CHECK(driver.status == ADXL345_STATUS_RECOVERY_REQUIRED);
  CHECK(driver.recovery_request_count == 1U);
  bus.next_result = ADXL345_TRANSPORT_TIMEOUT;
  CHECK(adxl345_service(&driver, 1U, 0U));
  CHECK(driver.last_error_status == ADXL345_STATUS_TRANSPORT_TIMEOUT);
  CHECK(driver.status == ADXL345_STATUS_OFFLINE);

  mock_initialize(&bus);
  CHECK(adxl345_initialize(&driver, &ops, &config));
  while (driver.state != ADXL345_STATE_VERIFY_DATA_FORMAT)
  {
    CHECK(adxl345_service(&driver, 0U, 0U));
  }
  bus.registers[ADXL345_DATA_FORMAT_REGISTER] = 0U;
  CHECK(adxl345_service(&driver, 0U, 0U));
  CHECK(driver.last_error_status ==
        ADXL345_STATUS_CONFIGURATION_MISMATCH);
  CHECK(driver.status == ADXL345_STATUS_RECOVERY_REQUIRED);
  return EXIT_SUCCESS;
}

static int test_stall_recovery_and_tick_wrap(void)
{
  mock_bus_t bus;
  adxl345_t driver;
  const adxl345_config_t config = {{0, 0, 0}, false};
  mock_initialize(&bus);
  CHECK(initialize_driver(&driver, &bus, &config, 1000U));
  CHECK(adxl345_service(&driver, 1099U, 0U));
  CHECK(driver.state == ADXL345_STATE_WAIT_DATA_READY);
  CHECK(adxl345_service(&driver, 1100U, 0U));
  CHECK(driver.last_error_status == ADXL345_STATUS_DATA_READY_STALLED);
  CHECK(driver.status == ADXL345_STATUS_RECOVERY_REQUIRED);

  mock_initialize(&bus);
  CHECK(initialize_driver(&driver, &bus, &config, UINT32_C(0xfffffff0)));
  CHECK(driver.data_ready_deadline_ms == UINT32_C(84));
  CHECK(adxl345_service(&driver, 83U, 0U));
  CHECK(driver.state == ADXL345_STATE_WAIT_DATA_READY);
  CHECK(adxl345_service(&driver, 84U, 0U));
  CHECK(driver.last_error_status == ADXL345_STATUS_DATA_READY_STALLED);
  return EXIT_SUCCESS;
}

static int test_read_error_gap_and_recovery_success(void)
{
  mock_bus_t bus;
  adxl345_t driver;
  const adxl345_config_t config = {{0, 0, 0}, false};
  mock_initialize(&bus);
  CHECK(initialize_driver(&driver, &bus, &config, 0U));
  bus.next_result = ADXL345_TRANSPORT_IO_ERROR;
  CHECK(adxl345_service(&driver, 10U, 1U));
  CHECK(driver.last_error_status == ADXL345_STATUS_TRANSPORT_IO_ERROR);
  CHECK(driver.dropped_sample_lower_bound == 1U);
  CHECK(driver.recovery_request_count == 1U);

  uint32_t guard = 0U;
  while ((driver.state != ADXL345_STATE_WAIT_DATA_READY) && (guard < 20U))
  {
    CHECK(adxl345_service(&driver, 20U + guard, 0U));
    ++guard;
  }
  CHECK(driver.state == ADXL345_STATE_WAIT_DATA_READY);
  set_sample(&bus, 0, 0, 256);
  CHECK(adxl345_service(&driver, 50U, 1U));
  CHECK(driver.status == ADXL345_STATUS_VALID);
  CHECK(driver.recovery_success_count == 1U);
  CHECK((driver.sample.quality_flags & ADXL345_QUALITY_GAP) != 0U);
  CHECK(driver.sample.dropped_sample_lower_bound == 1U);
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(test_parse_and_scale() == EXIT_SUCCESS);
  CHECK(test_configuration_sequence_and_bias() == EXIT_SUCCESS);
  CHECK(test_feature_windows() == EXIT_SUCCESS);
  CHECK(test_event_admission_and_drop_lower_bound() == EXIT_SUCCESS);
  CHECK(test_identity_configuration_and_transport_failures() == EXIT_SUCCESS);
  CHECK(test_stall_recovery_and_tick_wrap() == EXIT_SUCCESS);
  CHECK(test_read_error_gap_and_recovery_success() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
