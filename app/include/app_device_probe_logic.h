#ifndef APP_DEVICE_PROBE_LOGIC_H
#define APP_DEVICE_PROBE_LOGIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_DEVICE_PROBE_BME280_ID_REGISTER UINT8_C(0xd0)
#define APP_DEVICE_PROBE_BME280_EXPECTED_ID UINT8_C(0x60)
#define APP_DEVICE_PROBE_ADXL345_ID_REGISTER UINT8_C(0x00)
#define APP_DEVICE_PROBE_ADXL345_EXPECTED_ID UINT8_C(0xe5)
#define APP_DEVICE_PROBE_VEML7700_ADDRESS_7BIT UINT8_C(0x10)
#define APP_DEVICE_PROBE_VEML7700_CONFIG_REGISTER UINT8_C(0x00)

typedef enum
{
  APP_DEVICE_PROBE_DEVICE_BME280 = 0,
  APP_DEVICE_PROBE_DEVICE_ADXL345,
  APP_DEVICE_PROBE_DEVICE_VEML7700,
  APP_DEVICE_PROBE_DEVICE_COUNT
} app_device_probe_device_t;

typedef enum
{
  APP_DEVICE_PROBE_BUS_SPI = 0,
  APP_DEVICE_PROBE_BUS_I2C,
  APP_DEVICE_PROBE_BUS_COUNT
} app_device_probe_bus_t;

typedef enum
{
  APP_DEVICE_PROBE_TRANSPORT_OK = 0,
  APP_DEVICE_PROBE_TRANSPORT_NOT_PRESENT,
  APP_DEVICE_PROBE_TRANSPORT_TIMEOUT,
  APP_DEVICE_PROBE_TRANSPORT_BUS_ERROR
} app_device_probe_transport_result_t;

typedef enum
{
  APP_DEVICE_PROBE_STATUS_UNTESTED = 0,
  APP_DEVICE_PROBE_STATUS_IDENTITY_OK,
  APP_DEVICE_PROBE_STATUS_PRESENCE_OK,
  APP_DEVICE_PROBE_STATUS_NOT_PRESENT,
  APP_DEVICE_PROBE_STATUS_WRONG_ID,
  APP_DEVICE_PROBE_STATUS_TIMEOUT,
  APP_DEVICE_PROBE_STATUS_BUS_ERROR,
  APP_DEVICE_PROBE_STATUS_RECOVERY_REQUIRED,
  APP_DEVICE_PROBE_STATUS_INVALID_ARGUMENT
} app_device_probe_status_t;

typedef struct
{
  app_device_probe_status_t status;
  app_device_probe_transport_result_t last_transport_result;
  uint16_t observed_value;
  uint8_t attempts;
  bool recovery_requested;
} app_device_probe_entry_t;

typedef struct
{
  app_device_probe_entry_t devices[APP_DEVICE_PROBE_DEVICE_COUNT];
} app_device_probe_summary_t;

typedef struct
{
  void *context;
  app_device_probe_transport_result_t (*spi_read_register)(
      void *context,
      app_device_probe_device_t device,
      uint8_t register_address,
      uint8_t *value);
  app_device_probe_transport_result_t (*i2c_is_device_ready)(
      void *context,
      uint8_t address_7bit);
  app_device_probe_transport_result_t (*i2c_read_register)(
      void *context,
      uint8_t address_7bit,
      uint8_t register_address,
      uint8_t *data,
      size_t length);
  bool (*request_recovery)(void *context, app_device_probe_bus_t bus);
} app_device_probe_ops_t;

bool app_device_probe_run(const app_device_probe_ops_t *ops,
                          app_device_probe_summary_t *summary);
const char *app_device_probe_status_token(app_device_probe_status_t status);

#endif
