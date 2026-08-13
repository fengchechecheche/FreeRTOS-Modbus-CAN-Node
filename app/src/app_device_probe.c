#include "app_device_probe.h"

#include <stdio.h>
#include <string.h>

#include "bsp_i2c_bus.h"
#include "bsp_spi_bus.h"
#include "usart.h"

#define APP_DEVICE_PROBE_REPORT_CAPACITY (192U)
#define APP_DEVICE_PROBE_UART_TIMEOUT_MS (100U)

static app_device_probe_summary_t app_device_probe_summary;

static app_device_probe_transport_result_t app_device_probe_map_spi_result(
    bsp_spi_bus_result_t result)
{
  switch (result)
  {
    case BSP_SPI_BUS_RESULT_OK:
      return APP_DEVICE_PROBE_TRANSPORT_OK;
    case BSP_SPI_BUS_RESULT_TIMEOUT:
      return APP_DEVICE_PROBE_TRANSPORT_TIMEOUT;
    case BSP_SPI_BUS_RESULT_INVALID_ARGUMENT:
    case BSP_SPI_BUS_RESULT_BUSY:
    case BSP_SPI_BUS_RESULT_IO_ERROR:
    default:
      return APP_DEVICE_PROBE_TRANSPORT_BUS_ERROR;
  }
}

static app_device_probe_transport_result_t app_device_probe_map_i2c_result(
    bsp_i2c_bus_result_t result)
{
  switch (result)
  {
    case BSP_I2C_BUS_RESULT_OK:
      return APP_DEVICE_PROBE_TRANSPORT_OK;
    case BSP_I2C_BUS_RESULT_NOT_PRESENT:
      return APP_DEVICE_PROBE_TRANSPORT_NOT_PRESENT;
    case BSP_I2C_BUS_RESULT_TIMEOUT:
      return APP_DEVICE_PROBE_TRANSPORT_TIMEOUT;
    case BSP_I2C_BUS_RESULT_INVALID_ARGUMENT:
    case BSP_I2C_BUS_RESULT_BUSY:
    case BSP_I2C_BUS_RESULT_IO_ERROR:
    default:
      return APP_DEVICE_PROBE_TRANSPORT_BUS_ERROR;
  }
}

static app_device_probe_transport_result_t app_device_probe_spi_read(
    void *context,
    app_device_probe_device_t device,
    uint8_t register_address,
    uint8_t *value)
{
  (void)context;
  const bsp_spi_device_t bsp_device =
      device == APP_DEVICE_PROBE_DEVICE_BME280 ? BSP_SPI_DEVICE_BME280
                                                : BSP_SPI_DEVICE_ADXL345;
  return app_device_probe_map_spi_result(bsp_spi_bus_read_register(
      bsp_device,
      register_address,
      value,
      BSP_SPI_BUS_DEFAULT_TIMEOUT_MS));
}

static app_device_probe_transport_result_t app_device_probe_i2c_ready(
    void *context,
    uint8_t address_7bit)
{
  (void)context;
  return app_device_probe_map_i2c_result(bsp_i2c_bus_is_device_ready(
      address_7bit,
      BSP_I2C_BUS_DEFAULT_TIMEOUT_MS));
}

static app_device_probe_transport_result_t app_device_probe_i2c_read(
    void *context,
    uint8_t address_7bit,
    uint8_t register_address,
    uint8_t *data,
    size_t length)
{
  (void)context;
  return app_device_probe_map_i2c_result(bsp_i2c_bus_read_register(
      address_7bit,
      register_address,
      data,
      length,
      BSP_I2C_BUS_DEFAULT_TIMEOUT_MS));
}

static bool app_device_probe_request_recovery(void *context,
                                              app_device_probe_bus_t bus)
{
  (void)context;
  (void)bus;
  return false;
}

void app_device_probe_initialize(void)
{
  (void)memset(&app_device_probe_summary, 0, sizeof(app_device_probe_summary));
}

void app_device_probe_run_once(void)
{
  const app_device_probe_ops_t ops = {
      .context = NULL,
      .spi_read_register = app_device_probe_spi_read,
      .i2c_is_device_ready = app_device_probe_i2c_ready,
      .i2c_read_register = app_device_probe_i2c_read,
      .request_recovery = app_device_probe_request_recovery,
  };
  (void)app_device_probe_run(&ops, &app_device_probe_summary);

  const app_device_probe_entry_t *bme =
      &app_device_probe_summary.devices[APP_DEVICE_PROBE_DEVICE_BME280];
  const app_device_probe_entry_t *adxl =
      &app_device_probe_summary.devices[APP_DEVICE_PROBE_DEVICE_ADXL345];
  const app_device_probe_entry_t *veml =
      &app_device_probe_summary.devices[APP_DEVICE_PROBE_DEVICE_VEML7700];
  char report[APP_DEVICE_PROBE_REPORT_CAPACITY];
  const int written = snprintf(
      report,
      sizeof(report),
      "P5 S2 T04 BME=%02X/%s ADXL=%02X/%s VEML=10/%s\r\n",
      (unsigned int)bme->observed_value,
      app_device_probe_status_token(bme->status),
      (unsigned int)adxl->observed_value,
      app_device_probe_status_token(adxl->status),
      app_device_probe_status_token(veml->status));
  if (written > 0)
  {
    const size_t report_length =
        (size_t)written < sizeof(report) ? (size_t)written
                                         : (sizeof(report) - 1U);
    (void)HAL_UART_Transmit(&huart2,
                            (uint8_t *)report,
                            (uint16_t)report_length,
                            APP_DEVICE_PROBE_UART_TIMEOUT_MS);
  }
}

const app_device_probe_summary_t *app_device_probe_get_summary(void)
{
  return &app_device_probe_summary;
}
