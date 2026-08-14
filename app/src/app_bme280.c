#include "app_bme280.h"

#include <string.h>

#include "bsp_spi_bus.h"

static bme280_t app_bme280_driver;
static app_bme280_snapshot_t app_bme280_snapshot;

static bme280_transport_result_t app_bme280_map_transport(
    bsp_spi_bus_result_t result)
{
  switch (result)
  {
    case BSP_SPI_BUS_RESULT_OK:
      return BME280_TRANSPORT_OK;
    case BSP_SPI_BUS_RESULT_INVALID_ARGUMENT:
      return BME280_TRANSPORT_INVALID_ARGUMENT;
    case BSP_SPI_BUS_RESULT_BUSY:
      return BME280_TRANSPORT_BUSY;
    case BSP_SPI_BUS_RESULT_TIMEOUT:
      return BME280_TRANSPORT_TIMEOUT;
    case BSP_SPI_BUS_RESULT_IO_ERROR:
    default:
      return BME280_TRANSPORT_IO_ERROR;
  }
}

static bme280_transport_result_t app_bme280_read(void *context,
                                                 uint8_t register_address,
                                                 uint8_t *data,
                                                 size_t length)
{
  (void)context;
  return app_bme280_map_transport(bsp_spi_bus_read_registers(
      BSP_SPI_DEVICE_BME280,
      register_address,
      data,
      length,
      APP_BME280_SPI_TIMEOUT_MS));
}

static bme280_transport_result_t app_bme280_write(void *context,
                                                  uint8_t register_address,
                                                  uint8_t value)
{
  (void)context;
  return app_bme280_map_transport(bsp_spi_bus_write_register(
      BSP_SPI_DEVICE_BME280,
      register_address,
      value,
      APP_BME280_SPI_TIMEOUT_MS));
}

static void app_bme280_update_snapshot(void)
{
  app_bme280_snapshot.state = app_bme280_driver.state;
  app_bme280_snapshot.status = app_bme280_driver.status;
  app_bme280_snapshot.last_transport_result =
      app_bme280_driver.last_transport_result;
  app_bme280_snapshot.sample = app_bme280_driver.sample;
  app_bme280_snapshot.transaction_count = app_bme280_driver.transaction_count;
  app_bme280_snapshot.error_count = app_bme280_driver.error_count;
  app_bme280_snapshot.recovery_request_count =
      app_bme280_driver.recovery_request_count;
  app_bme280_snapshot.recovery_success_count =
      app_bme280_driver.recovery_success_count;
}

void app_bme280_initialize(void)
{
  const bme280_ops_t ops = {
      .context = NULL,
      .read = app_bme280_read,
      .write = app_bme280_write,
  };
  (void)memset(&app_bme280_snapshot, 0, sizeof(app_bme280_snapshot));
  (void)bme280_initialize(
      &app_bme280_driver, &ops, BME280_DEFAULT_SAMPLE_PERIOD_MS);
  app_bme280_update_snapshot();
}

void app_bme280_service(uint32_t now_ms)
{
  (void)bme280_service(&app_bme280_driver, now_ms);
  app_bme280_update_snapshot();
}

bool app_bme280_get_snapshot(app_bme280_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return false;
  }
  *snapshot = app_bme280_snapshot;
  return true;
}
