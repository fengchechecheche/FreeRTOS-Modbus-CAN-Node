#include "app_device_probe_logic.h"

#include <stdbool.h>
#include <stddef.h>
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
  app_device_probe_transport_result_t spi_results[2][2];
  uint8_t spi_values[2][2];
  size_t spi_calls[2];
  app_device_probe_transport_result_t i2c_ready_results[2];
  app_device_probe_transport_result_t i2c_read_results[2];
  uint8_t i2c_values[2][2];
  size_t i2c_ready_calls;
  size_t i2c_read_calls;
  bool recovery_ok[APP_DEVICE_PROBE_BUS_COUNT];
  size_t recovery_calls[APP_DEVICE_PROBE_BUS_COUNT];
} mock_transport_t;

static void mock_initialize(mock_transport_t *mock)
{
  (void)memset(mock, 0, sizeof(*mock));
  for (size_t device = 0U; device < 2U; ++device)
  {
    for (size_t attempt = 0U; attempt < 2U; ++attempt)
    {
      mock->spi_results[device][attempt] = APP_DEVICE_PROBE_TRANSPORT_OK;
    }
  }
  mock->spi_values[APP_DEVICE_PROBE_DEVICE_BME280][0] =
      APP_DEVICE_PROBE_BME280_EXPECTED_ID;
  mock->spi_values[APP_DEVICE_PROBE_DEVICE_BME280][1] =
      APP_DEVICE_PROBE_BME280_EXPECTED_ID;
  mock->spi_values[APP_DEVICE_PROBE_DEVICE_ADXL345][0] =
      APP_DEVICE_PROBE_ADXL345_EXPECTED_ID;
  mock->spi_values[APP_DEVICE_PROBE_DEVICE_ADXL345][1] =
      APP_DEVICE_PROBE_ADXL345_EXPECTED_ID;
  for (size_t attempt = 0U; attempt < 2U; ++attempt)
  {
    mock->i2c_ready_results[attempt] = APP_DEVICE_PROBE_TRANSPORT_OK;
    mock->i2c_read_results[attempt] = APP_DEVICE_PROBE_TRANSPORT_OK;
    mock->i2c_values[attempt][0] = UINT8_C(0x01);
    mock->i2c_values[attempt][1] = UINT8_C(0x00);
  }
}

static size_t mock_attempt_index(size_t calls)
{
  return calls < 2U ? calls : 1U;
}

static app_device_probe_transport_result_t mock_spi_read(
    void *context,
    app_device_probe_device_t device,
    uint8_t register_address,
    uint8_t *value)
{
  mock_transport_t *mock = context;
  if ((device != APP_DEVICE_PROBE_DEVICE_BME280) &&
      (device != APP_DEVICE_PROBE_DEVICE_ADXL345))
  {
    return APP_DEVICE_PROBE_TRANSPORT_BUS_ERROR;
  }
  if (((device == APP_DEVICE_PROBE_DEVICE_BME280) &&
       (register_address != APP_DEVICE_PROBE_BME280_ID_REGISTER)) ||
      ((device == APP_DEVICE_PROBE_DEVICE_ADXL345) &&
       (register_address != APP_DEVICE_PROBE_ADXL345_ID_REGISTER)))
  {
    return APP_DEVICE_PROBE_TRANSPORT_BUS_ERROR;
  }

  const size_t device_index = (size_t)device;
  const size_t attempt = mock_attempt_index(mock->spi_calls[device_index]);
  ++mock->spi_calls[device_index];
  const app_device_probe_transport_result_t result =
      mock->spi_results[device_index][attempt];
  if (result == APP_DEVICE_PROBE_TRANSPORT_OK)
  {
    *value = mock->spi_values[device_index][attempt];
  }
  return result;
}

static app_device_probe_transport_result_t mock_i2c_ready(
    void *context,
    uint8_t address_7bit)
{
  mock_transport_t *mock = context;
  if (address_7bit != APP_DEVICE_PROBE_VEML7700_ADDRESS_7BIT)
  {
    return APP_DEVICE_PROBE_TRANSPORT_BUS_ERROR;
  }
  const size_t attempt = mock_attempt_index(mock->i2c_ready_calls);
  ++mock->i2c_ready_calls;
  return mock->i2c_ready_results[attempt];
}

static app_device_probe_transport_result_t mock_i2c_read(
    void *context,
    uint8_t address_7bit,
    uint8_t register_address,
    uint8_t *data,
    size_t length)
{
  mock_transport_t *mock = context;
  if ((address_7bit != APP_DEVICE_PROBE_VEML7700_ADDRESS_7BIT) ||
      (register_address != APP_DEVICE_PROBE_VEML7700_CONFIG_REGISTER) ||
      (length != 2U))
  {
    return APP_DEVICE_PROBE_TRANSPORT_BUS_ERROR;
  }
  const size_t attempt = mock_attempt_index(mock->i2c_read_calls);
  ++mock->i2c_read_calls;
  const app_device_probe_transport_result_t result =
      mock->i2c_read_results[attempt];
  if (result == APP_DEVICE_PROBE_TRANSPORT_OK)
  {
    data[0] = mock->i2c_values[attempt][0];
    data[1] = mock->i2c_values[attempt][1];
  }
  return result;
}

static bool mock_recovery(void *context, app_device_probe_bus_t bus)
{
  mock_transport_t *mock = context;
  if (bus >= APP_DEVICE_PROBE_BUS_COUNT)
  {
    return false;
  }
  ++mock->recovery_calls[bus];
  return mock->recovery_ok[bus];
}

static app_device_probe_ops_t mock_ops(mock_transport_t *mock)
{
  const app_device_probe_ops_t ops = {
      .context = mock,
      .spi_read_register = mock_spi_read,
      .i2c_is_device_ready = mock_i2c_ready,
      .i2c_read_register = mock_i2c_read,
      .request_recovery = mock_recovery,
  };
  return ops;
}

static int test_success_and_veml_presence_boundary(void)
{
  mock_transport_t mock;
  mock_initialize(&mock);
  const app_device_probe_ops_t ops = mock_ops(&mock);
  app_device_probe_summary_t summary;
  CHECK(app_device_probe_run(&ops, &summary));
  CHECK(summary.devices[APP_DEVICE_PROBE_DEVICE_BME280].status ==
        APP_DEVICE_PROBE_STATUS_IDENTITY_OK);
  CHECK(summary.devices[APP_DEVICE_PROBE_DEVICE_BME280].observed_value ==
        APP_DEVICE_PROBE_BME280_EXPECTED_ID);
  CHECK(summary.devices[APP_DEVICE_PROBE_DEVICE_ADXL345].status ==
        APP_DEVICE_PROBE_STATUS_IDENTITY_OK);
  CHECK(summary.devices[APP_DEVICE_PROBE_DEVICE_VEML7700].status ==
        APP_DEVICE_PROBE_STATUS_PRESENCE_OK);
  CHECK(summary.devices[APP_DEVICE_PROBE_DEVICE_VEML7700].observed_value ==
        UINT16_C(0x0001));
  CHECK(strcmp(app_device_probe_status_token(
                   summary.devices[APP_DEVICE_PROBE_DEVICE_VEML7700].status),
               "PRESENT") == 0);
  return EXIT_SUCCESS;
}

static int test_wrong_identity(void)
{
  mock_transport_t mock;
  mock_initialize(&mock);
  mock.spi_values[APP_DEVICE_PROBE_DEVICE_BME280][0] = UINT8_C(0x58);
  mock.spi_values[APP_DEVICE_PROBE_DEVICE_ADXL345][0] = UINT8_C(0x42);
  const app_device_probe_ops_t ops = mock_ops(&mock);
  app_device_probe_summary_t summary;
  CHECK(app_device_probe_run(&ops, &summary));
  CHECK(summary.devices[APP_DEVICE_PROBE_DEVICE_BME280].status ==
        APP_DEVICE_PROBE_STATUS_WRONG_ID);
  CHECK(summary.devices[APP_DEVICE_PROBE_DEVICE_ADXL345].status ==
        APP_DEVICE_PROBE_STATUS_WRONG_ID);
  CHECK(summary.devices[APP_DEVICE_PROBE_DEVICE_VEML7700].status ==
        APP_DEVICE_PROBE_STATUS_PRESENCE_OK);
  return EXIT_SUCCESS;
}

static int test_missing_devices_are_independent(void)
{
  mock_transport_t mock;
  mock_initialize(&mock);
  mock.spi_values[APP_DEVICE_PROBE_DEVICE_BME280][0] = UINT8_C(0xff);
  mock.spi_values[APP_DEVICE_PROBE_DEVICE_ADXL345][0] = UINT8_C(0x00);
  mock.i2c_ready_results[0] = APP_DEVICE_PROBE_TRANSPORT_NOT_PRESENT;
  const app_device_probe_ops_t ops = mock_ops(&mock);
  app_device_probe_summary_t summary;
  CHECK(app_device_probe_run(&ops, &summary));
  CHECK(summary.devices[APP_DEVICE_PROBE_DEVICE_BME280].status ==
        APP_DEVICE_PROBE_STATUS_NOT_PRESENT);
  CHECK(summary.devices[APP_DEVICE_PROBE_DEVICE_ADXL345].status ==
        APP_DEVICE_PROBE_STATUS_NOT_PRESENT);
  CHECK(summary.devices[APP_DEVICE_PROBE_DEVICE_VEML7700].status ==
        APP_DEVICE_PROBE_STATUS_NOT_PRESENT);
  CHECK(mock.i2c_read_calls == 0U);
  return EXIT_SUCCESS;
}

static int test_one_recovery_then_success(void)
{
  mock_transport_t mock;
  mock_initialize(&mock);
  mock.spi_results[APP_DEVICE_PROBE_DEVICE_BME280][0] =
      APP_DEVICE_PROBE_TRANSPORT_TIMEOUT;
  mock.recovery_ok[APP_DEVICE_PROBE_BUS_SPI] = true;
  const app_device_probe_ops_t ops = mock_ops(&mock);
  app_device_probe_summary_t summary;
  CHECK(app_device_probe_run(&ops, &summary));
  const app_device_probe_entry_t *bme =
      &summary.devices[APP_DEVICE_PROBE_DEVICE_BME280];
  CHECK(bme->status == APP_DEVICE_PROBE_STATUS_IDENTITY_OK);
  CHECK(bme->attempts == 2U);
  CHECK(bme->recovery_requested);
  CHECK(mock.recovery_calls[APP_DEVICE_PROBE_BUS_SPI] == 1U);
  CHECK(mock.spi_calls[APP_DEVICE_PROBE_DEVICE_BME280] == 2U);
  return EXIT_SUCCESS;
}

static int test_recovery_required_is_bounded(void)
{
  mock_transport_t mock;
  mock_initialize(&mock);
  mock.spi_results[APP_DEVICE_PROBE_DEVICE_ADXL345][0] =
      APP_DEVICE_PROBE_TRANSPORT_BUS_ERROR;
  mock.recovery_ok[APP_DEVICE_PROBE_BUS_SPI] = false;
  const app_device_probe_ops_t ops = mock_ops(&mock);
  app_device_probe_summary_t summary;
  CHECK(app_device_probe_run(&ops, &summary));
  const app_device_probe_entry_t *adxl =
      &summary.devices[APP_DEVICE_PROBE_DEVICE_ADXL345];
  CHECK(adxl->status == APP_DEVICE_PROBE_STATUS_RECOVERY_REQUIRED);
  CHECK(adxl->attempts == 1U);
  CHECK(adxl->last_transport_result ==
        APP_DEVICE_PROBE_TRANSPORT_BUS_ERROR);
  CHECK(mock.recovery_calls[APP_DEVICE_PROBE_BUS_SPI] == 1U);
  CHECK(mock.spi_calls[APP_DEVICE_PROBE_DEVICE_ADXL345] == 1U);
  return EXIT_SUCCESS;
}

static int test_i2c_retry_failure_stops(void)
{
  mock_transport_t mock;
  mock_initialize(&mock);
  mock.i2c_ready_results[0] = APP_DEVICE_PROBE_TRANSPORT_TIMEOUT;
  mock.i2c_ready_results[1] = APP_DEVICE_PROBE_TRANSPORT_TIMEOUT;
  mock.recovery_ok[APP_DEVICE_PROBE_BUS_I2C] = true;
  const app_device_probe_ops_t ops = mock_ops(&mock);
  app_device_probe_summary_t summary;
  CHECK(app_device_probe_run(&ops, &summary));
  const app_device_probe_entry_t *veml =
      &summary.devices[APP_DEVICE_PROBE_DEVICE_VEML7700];
  CHECK(veml->status == APP_DEVICE_PROBE_STATUS_TIMEOUT);
  CHECK(veml->attempts == 2U);
  CHECK(veml->recovery_requested);
  CHECK(mock.recovery_calls[APP_DEVICE_PROBE_BUS_I2C] == 1U);
  CHECK(mock.i2c_ready_calls == 2U);
  CHECK(mock.i2c_read_calls == 0U);
  return EXIT_SUCCESS;
}

static int test_invalid_ops(void)
{
  app_device_probe_ops_t ops;
  (void)memset(&ops, 0, sizeof(ops));
  app_device_probe_summary_t summary;
  CHECK(!app_device_probe_run(&ops, &summary));
  for (size_t index = 0U; index < APP_DEVICE_PROBE_DEVICE_COUNT; ++index)
  {
    CHECK(summary.devices[index].status ==
          APP_DEVICE_PROBE_STATUS_INVALID_ARGUMENT);
  }
  CHECK(!app_device_probe_run(NULL, &summary));
  CHECK(!app_device_probe_run(&ops, NULL));
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(test_success_and_veml_presence_boundary() == EXIT_SUCCESS);
  CHECK(test_wrong_identity() == EXIT_SUCCESS);
  CHECK(test_missing_devices_are_independent() == EXIT_SUCCESS);
  CHECK(test_one_recovery_then_success() == EXIT_SUCCESS);
  CHECK(test_recovery_required_is_bounded() == EXIT_SUCCESS);
  CHECK(test_i2c_retry_failure_stops() == EXIT_SUCCESS);
  CHECK(test_invalid_ops() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
