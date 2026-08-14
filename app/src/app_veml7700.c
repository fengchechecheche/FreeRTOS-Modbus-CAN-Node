#include "app_veml7700.h"

#include <string.h>

#include "bsp_i2c_bus.h"

static veml7700_t app_veml7700_driver;
static app_veml7700_snapshot_t app_veml7700_snapshot;

static veml7700_transport_result_t app_veml7700_map_transport(
    bsp_i2c_bus_result_t result)
{
  switch (result)
  {
    case BSP_I2C_BUS_RESULT_OK:
      return VEML7700_TRANSPORT_OK;
    case BSP_I2C_BUS_RESULT_INVALID_ARGUMENT:
      return VEML7700_TRANSPORT_INVALID_ARGUMENT;
    case BSP_I2C_BUS_RESULT_NOT_PRESENT:
      return VEML7700_TRANSPORT_NOT_PRESENT;
    case BSP_I2C_BUS_RESULT_BUSY:
      return VEML7700_TRANSPORT_BUSY;
    case BSP_I2C_BUS_RESULT_TIMEOUT:
      return VEML7700_TRANSPORT_TIMEOUT;
    case BSP_I2C_BUS_RESULT_IO_ERROR:
    default:
      return VEML7700_TRANSPORT_IO_ERROR;
  }
}

static veml7700_transport_result_t app_veml7700_read(
    void *context,
    uint8_t register_address,
    uint8_t *data,
    size_t length)
{
  (void)context;
  return app_veml7700_map_transport(bsp_i2c_bus_read_register(
      VEML7700_ADDRESS_7BIT,
      register_address,
      data,
      length,
      APP_VEML7700_I2C_TIMEOUT_MS));
}

static veml7700_transport_result_t app_veml7700_write(
    void *context,
    uint8_t register_address,
    const uint8_t *data,
    size_t length)
{
  (void)context;
  return app_veml7700_map_transport(bsp_i2c_bus_write_register(
      VEML7700_ADDRESS_7BIT,
      register_address,
      data,
      length,
      APP_VEML7700_I2C_TIMEOUT_MS));
}

static bool app_veml7700_request_recovery(void *context)
{
  (void)context;
  return false;
}

static void app_veml7700_update_snapshot(void)
{
  app_veml7700_snapshot.state = app_veml7700_driver.state;
  app_veml7700_snapshot.status = app_veml7700_driver.status;
  app_veml7700_snapshot.last_error_status =
      app_veml7700_driver.last_error_status;
  app_veml7700_snapshot.last_transport_result =
      app_veml7700_driver.last_transport_result;
  app_veml7700_snapshot.sample = app_veml7700_driver.sample;
  app_veml7700_snapshot.transaction_count =
      app_veml7700_driver.transaction_count;
  app_veml7700_snapshot.error_count = app_veml7700_driver.error_count;
  app_veml7700_snapshot.range_change_count =
      app_veml7700_driver.range_change_count;
  app_veml7700_snapshot.recovery_request_count =
      app_veml7700_driver.recovery_request_count;
  app_veml7700_snapshot.recovery_success_count =
      app_veml7700_driver.recovery_success_count;
}

void app_veml7700_initialize(void)
{
  const veml7700_ops_t ops = {
      .context = NULL,
      .read = app_veml7700_read,
      .write = app_veml7700_write,
      .request_recovery = app_veml7700_request_recovery,
  };
  (void)memset(&app_veml7700_snapshot, 0, sizeof(app_veml7700_snapshot));
  (void)veml7700_initialize(
      &app_veml7700_driver, &ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS);
  app_veml7700_update_snapshot();
}

void app_veml7700_service(uint32_t now_ms)
{
  (void)veml7700_service(&app_veml7700_driver, now_ms);
  app_veml7700_update_snapshot();
}

bool app_veml7700_get_snapshot(app_veml7700_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return false;
  }
  *snapshot = app_veml7700_snapshot;
  return true;
}
