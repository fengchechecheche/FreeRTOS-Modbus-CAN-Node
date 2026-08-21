#include "app_adxl345.h"

#include <string.h>

#include "bsp_spi_bus.h"

#define APP_ADXL345_INT_SOURCE_REGISTER UINT8_C(0x30)

static adxl345_t app_adxl345_driver;
static app_adxl345_snapshot_t app_adxl345_snapshot;
static app_adxl345_register_diagnostic_t
    app_adxl345_register_diagnostic;
static app_adxl345_polling_diagnostic_t
    app_adxl345_polling_diagnostic;

static adxl345_transport_result_t app_adxl345_map_transport(
    bsp_spi_bus_result_t result)
{
  switch (result)
  {
    case BSP_SPI_BUS_RESULT_OK:
      return ADXL345_TRANSPORT_OK;
    case BSP_SPI_BUS_RESULT_INVALID_ARGUMENT:
      return ADXL345_TRANSPORT_INVALID_ARGUMENT;
    case BSP_SPI_BUS_RESULT_BUSY:
      return ADXL345_TRANSPORT_BUSY;
    case BSP_SPI_BUS_RESULT_TIMEOUT:
      return ADXL345_TRANSPORT_TIMEOUT;
    case BSP_SPI_BUS_RESULT_IO_ERROR:
    default:
      return ADXL345_TRANSPORT_IO_ERROR;
  }
}

static adxl345_transport_result_t app_adxl345_read(
    void *context,
    uint8_t register_address,
    uint8_t *data,
    size_t length)
{
  (void)context;
  return app_adxl345_map_transport(bsp_spi_bus_read_registers(
      BSP_SPI_DEVICE_ADXL345,
      register_address,
      data,
      length,
      APP_ADXL345_SPI_TIMEOUT_MS));
}

static adxl345_transport_result_t app_adxl345_write(
    void *context,
    uint8_t register_address,
    uint8_t value)
{
  (void)context;
  return app_adxl345_map_transport(bsp_spi_bus_write_register(
      BSP_SPI_DEVICE_ADXL345,
      register_address,
      value,
      APP_ADXL345_SPI_TIMEOUT_MS));
}

static void app_adxl345_update_snapshot(void)
{
  app_adxl345_snapshot.state = app_adxl345_driver.state;
  app_adxl345_snapshot.status = app_adxl345_driver.status;
  app_adxl345_snapshot.last_error_status =
      app_adxl345_driver.last_error_status;
  app_adxl345_snapshot.last_transport_result =
      app_adxl345_driver.last_transport_result;
  app_adxl345_snapshot.sample = app_adxl345_driver.sample;
  app_adxl345_snapshot.feature = app_adxl345_driver.feature;
  app_adxl345_snapshot.feature_window_count =
      app_adxl345_driver.window.count;
  app_adxl345_snapshot.transaction_count =
      app_adxl345_driver.transaction_count;
  app_adxl345_snapshot.error_count = app_adxl345_driver.error_count;
  app_adxl345_snapshot.recovery_request_count =
      app_adxl345_driver.recovery_request_count;
  app_adxl345_snapshot.recovery_success_count =
      app_adxl345_driver.recovery_success_count;
  app_adxl345_snapshot.irq_event_count =
      app_adxl345_driver.irq_event_count;
  app_adxl345_snapshot.dropped_sample_lower_bound =
      app_adxl345_driver.dropped_sample_lower_bound;
}

void app_adxl345_initialize(void)
{
  const adxl345_ops_t ops = {
      .context = NULL,
      .read = app_adxl345_read,
      .write = app_adxl345_write,
  };
  const adxl345_config_t config = {
      .bias_lsb = {0, 0, 0},
      .bias_calibrated = false,
  };
  (void)memset(&app_adxl345_snapshot, 0, sizeof(app_adxl345_snapshot));
  (void)memset(&app_adxl345_register_diagnostic,
               0,
               sizeof(app_adxl345_register_diagnostic));
  (void)memset(&app_adxl345_polling_diagnostic,
               0,
               sizeof(app_adxl345_polling_diagnostic));
  (void)adxl345_initialize(&app_adxl345_driver, &ops, &config);
  app_adxl345_update_snapshot();
}

void app_adxl345_service(uint32_t now_ms,
                         uint32_t data_ready_event_count)
{
#if P5_ADXL345_POLLING_FALLBACK_ENABLE
  if ((data_ready_event_count == 0U) &&
      (app_adxl345_driver.state == ADXL345_STATE_WAIT_DATA_READY))
  {
    uint8_t int_source = 0U;
    ++app_adxl345_polling_diagnostic.attempt_count;
    const bsp_spi_bus_result_t result = bsp_spi_bus_read_register(
        BSP_SPI_DEVICE_ADXL345,
        APP_ADXL345_INT_SOURCE_REGISTER,
        &int_source,
        APP_ADXL345_SPI_TIMEOUT_MS);
    if (result == BSP_SPI_BUS_RESULT_OK)
    {
      app_adxl345_polling_diagnostic.last_int_source = int_source;
      if ((int_source & ADXL345_INT_DATA_READY) != 0U)
      {
        ++app_adxl345_polling_diagnostic.ready_count;
        (void)adxl345_service_polled_data_ready(
            &app_adxl345_driver, now_ms);
        app_adxl345_update_snapshot();
        return;
      }
    }
    else
    {
      ++app_adxl345_polling_diagnostic.error_count;
    }
  }
#endif
  (void)adxl345_service(
      &app_adxl345_driver, now_ms, data_ready_event_count);
  app_adxl345_update_snapshot();
}

bool app_adxl345_get_snapshot(app_adxl345_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return false;
  }
  *snapshot = app_adxl345_snapshot;
  return true;
}

void app_adxl345_capture_register_diagnostic_once(void)
{
  if (app_adxl345_register_diagnostic.attempted ||
      (app_adxl345_driver.status != ADXL345_STATUS_OFFLINE))
  {
    return;
  }

  app_adxl345_register_diagnostic_t next = {
      .attempted = true,
      .valid = true,
      .power_ctl = UINT8_C(0xff),
      .int_enable = UINT8_C(0xff),
      .int_map = UINT8_C(0xff),
      .int_source = UINT8_C(0xff),
  };
  next.valid =
      (bsp_spi_bus_read_register(BSP_SPI_DEVICE_ADXL345,
                                 ADXL345_POWER_CTL_REGISTER,
                                 &next.power_ctl,
                                 APP_ADXL345_SPI_TIMEOUT_MS) ==
       BSP_SPI_BUS_RESULT_OK) &&
      next.valid;
  next.valid =
      (bsp_spi_bus_read_register(BSP_SPI_DEVICE_ADXL345,
                                 ADXL345_INT_ENABLE_REGISTER,
                                 &next.int_enable,
                                 APP_ADXL345_SPI_TIMEOUT_MS) ==
       BSP_SPI_BUS_RESULT_OK) &&
      next.valid;
  next.valid =
      (bsp_spi_bus_read_register(BSP_SPI_DEVICE_ADXL345,
                                 ADXL345_INT_MAP_REGISTER,
                                 &next.int_map,
                                 APP_ADXL345_SPI_TIMEOUT_MS) ==
       BSP_SPI_BUS_RESULT_OK) &&
      next.valid;
  next.valid =
      (bsp_spi_bus_read_register(BSP_SPI_DEVICE_ADXL345,
                                 APP_ADXL345_INT_SOURCE_REGISTER,
                                 &next.int_source,
                                 APP_ADXL345_SPI_TIMEOUT_MS) ==
       BSP_SPI_BUS_RESULT_OK) &&
      next.valid;
  app_adxl345_register_diagnostic = next;
}

bool app_adxl345_get_register_diagnostic(
    app_adxl345_register_diagnostic_t *diagnostic)
{
  if (diagnostic == NULL)
  {
    return false;
  }
  *diagnostic = app_adxl345_register_diagnostic;
  return true;
}

bool app_adxl345_get_polling_diagnostic(
    app_adxl345_polling_diagnostic_t *diagnostic)
{
  if (diagnostic == NULL)
  {
    return false;
  }
  *diagnostic = app_adxl345_polling_diagnostic;
  return true;
}
