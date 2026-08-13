#include "app_device_probe_logic.h"

#include <string.h>

#define APP_DEVICE_PROBE_MAX_ATTEMPTS (2U)

static app_device_probe_status_t app_device_probe_map_transport(
    app_device_probe_transport_result_t result)
{
  switch (result)
  {
    case APP_DEVICE_PROBE_TRANSPORT_NOT_PRESENT:
      return APP_DEVICE_PROBE_STATUS_NOT_PRESENT;
    case APP_DEVICE_PROBE_TRANSPORT_TIMEOUT:
      return APP_DEVICE_PROBE_STATUS_TIMEOUT;
    case APP_DEVICE_PROBE_TRANSPORT_BUS_ERROR:
      return APP_DEVICE_PROBE_STATUS_BUS_ERROR;
    case APP_DEVICE_PROBE_TRANSPORT_OK:
    default:
      return APP_DEVICE_PROBE_STATUS_BUS_ERROR;
  }
}

static bool app_device_probe_try_recovery(const app_device_probe_ops_t *ops,
                                          app_device_probe_bus_t bus,
                                          app_device_probe_entry_t *entry)
{
  entry->recovery_requested = true;
  if ((ops->request_recovery == NULL) ||
      !ops->request_recovery(ops->context, bus))
  {
    entry->status = APP_DEVICE_PROBE_STATUS_RECOVERY_REQUIRED;
    return false;
  }
  return true;
}

static void app_device_probe_run_spi_identity(
    const app_device_probe_ops_t *ops,
    app_device_probe_device_t device,
    uint8_t register_address,
    uint8_t expected_id,
    app_device_probe_entry_t *entry)
{
  for (uint8_t attempt = 1U; attempt <= APP_DEVICE_PROBE_MAX_ATTEMPTS;
       ++attempt)
  {
    uint8_t observed_id = 0U;
    const app_device_probe_transport_result_t result =
        ops->spi_read_register(ops->context,
                               device,
                               register_address,
                               &observed_id);
    entry->attempts = attempt;
    entry->last_transport_result = result;

    if (result == APP_DEVICE_PROBE_TRANSPORT_OK)
    {
      entry->observed_value = observed_id;
      if ((observed_id == UINT8_C(0x00)) ||
          (observed_id == UINT8_C(0xff)))
      {
        entry->status = APP_DEVICE_PROBE_STATUS_NOT_PRESENT;
      }
      else if (observed_id != expected_id)
      {
        entry->status = APP_DEVICE_PROBE_STATUS_WRONG_ID;
      }
      else
      {
        entry->status = APP_DEVICE_PROBE_STATUS_IDENTITY_OK;
      }
      return;
    }

    if (result == APP_DEVICE_PROBE_TRANSPORT_NOT_PRESENT)
    {
      entry->status = APP_DEVICE_PROBE_STATUS_NOT_PRESENT;
      return;
    }

    if ((attempt == 1U) &&
        app_device_probe_try_recovery(ops,
                                      APP_DEVICE_PROBE_BUS_SPI,
                                      entry))
    {
      continue;
    }
    if (entry->status != APP_DEVICE_PROBE_STATUS_RECOVERY_REQUIRED)
    {
      entry->status = app_device_probe_map_transport(result);
    }
    return;
  }
}

static app_device_probe_transport_result_t app_device_probe_veml7700_attempt(
    const app_device_probe_ops_t *ops,
    uint16_t *observed_value)
{
  app_device_probe_transport_result_t result =
      ops->i2c_is_device_ready(ops->context,
                               APP_DEVICE_PROBE_VEML7700_ADDRESS_7BIT);
  if (result != APP_DEVICE_PROBE_TRANSPORT_OK)
  {
    return result;
  }

  uint8_t bytes[2] = {0U, 0U};
  result = ops->i2c_read_register(
      ops->context,
      APP_DEVICE_PROBE_VEML7700_ADDRESS_7BIT,
      APP_DEVICE_PROBE_VEML7700_CONFIG_REGISTER,
      bytes,
      sizeof(bytes));
  if (result == APP_DEVICE_PROBE_TRANSPORT_OK)
  {
    *observed_value =
        (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
  }
  return result;
}

static void app_device_probe_run_veml7700(
    const app_device_probe_ops_t *ops,
    app_device_probe_entry_t *entry)
{
  for (uint8_t attempt = 1U; attempt <= APP_DEVICE_PROBE_MAX_ATTEMPTS;
       ++attempt)
  {
    uint16_t observed_value = 0U;
    const app_device_probe_transport_result_t result =
        app_device_probe_veml7700_attempt(ops, &observed_value);
    entry->attempts = attempt;
    entry->last_transport_result = result;

    if (result == APP_DEVICE_PROBE_TRANSPORT_OK)
    {
      entry->observed_value = observed_value;
      entry->status = APP_DEVICE_PROBE_STATUS_PRESENCE_OK;
      return;
    }
    if (result == APP_DEVICE_PROBE_TRANSPORT_NOT_PRESENT)
    {
      entry->status = APP_DEVICE_PROBE_STATUS_NOT_PRESENT;
      return;
    }
    if ((attempt == 1U) &&
        app_device_probe_try_recovery(ops,
                                      APP_DEVICE_PROBE_BUS_I2C,
                                      entry))
    {
      continue;
    }
    if (entry->status != APP_DEVICE_PROBE_STATUS_RECOVERY_REQUIRED)
    {
      entry->status = app_device_probe_map_transport(result);
    }
    return;
  }
}

bool app_device_probe_run(const app_device_probe_ops_t *ops,
                          app_device_probe_summary_t *summary)
{
  if (summary == NULL)
  {
    return false;
  }
  (void)memset(summary, 0, sizeof(*summary));

  if ((ops == NULL) || (ops->spi_read_register == NULL) ||
      (ops->i2c_is_device_ready == NULL) ||
      (ops->i2c_read_register == NULL))
  {
    for (size_t index = 0U; index < APP_DEVICE_PROBE_DEVICE_COUNT; ++index)
    {
      summary->devices[index].status =
          APP_DEVICE_PROBE_STATUS_INVALID_ARGUMENT;
    }
    return false;
  }

  app_device_probe_run_spi_identity(
      ops,
      APP_DEVICE_PROBE_DEVICE_BME280,
      APP_DEVICE_PROBE_BME280_ID_REGISTER,
      APP_DEVICE_PROBE_BME280_EXPECTED_ID,
      &summary->devices[APP_DEVICE_PROBE_DEVICE_BME280]);
  app_device_probe_run_spi_identity(
      ops,
      APP_DEVICE_PROBE_DEVICE_ADXL345,
      APP_DEVICE_PROBE_ADXL345_ID_REGISTER,
      APP_DEVICE_PROBE_ADXL345_EXPECTED_ID,
      &summary->devices[APP_DEVICE_PROBE_DEVICE_ADXL345]);
  app_device_probe_run_veml7700(
      ops,
      &summary->devices[APP_DEVICE_PROBE_DEVICE_VEML7700]);
  return true;
}

const char *app_device_probe_status_token(app_device_probe_status_t status)
{
  switch (status)
  {
    case APP_DEVICE_PROBE_STATUS_IDENTITY_OK:
      return "OK";
    case APP_DEVICE_PROBE_STATUS_PRESENCE_OK:
      return "PRESENT";
    case APP_DEVICE_PROBE_STATUS_NOT_PRESENT:
      return "NOT_PRESENT";
    case APP_DEVICE_PROBE_STATUS_WRONG_ID:
      return "WRONG_ID";
    case APP_DEVICE_PROBE_STATUS_TIMEOUT:
      return "TIMEOUT";
    case APP_DEVICE_PROBE_STATUS_BUS_ERROR:
      return "BUS_ERROR";
    case APP_DEVICE_PROBE_STATUS_RECOVERY_REQUIRED:
      return "RECOVERY_REQUIRED";
    case APP_DEVICE_PROBE_STATUS_INVALID_ARGUMENT:
      return "INVALID";
    case APP_DEVICE_PROBE_STATUS_UNTESTED:
    default:
      return "UNTESTED";
  }
}
