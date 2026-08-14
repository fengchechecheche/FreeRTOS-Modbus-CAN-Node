#ifndef APP_MODBUS_REGISTER_IMAGE_H
#define APP_MODBUS_REGISTER_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_health_policy.h"
#include "app_measurement.h"
#include "app_sensor_monitor.h"
#include "p5_modbus_server.h"

#define APP_MODBUS_REGISTER_MAP_REVISION (1U)
#define APP_MODBUS_DEVICE_SIGNATURE UINT16_C(0x5035)
#define APP_MODBUS_FIRMWARE_VERSION_MAJOR (0U)
#define APP_MODBUS_FIRMWARE_VERSION_MINOR (0U)
#define APP_MODBUS_FIRMWARE_VERSION_PATCH (0U)
#define APP_MODBUS_FIRMWARE_VERSION_FLAGS (0U)

typedef struct
{
  app_sensor_fault_class_t fault_class;
  uint32_t fault_episode_count;
  uint32_t recovery_request_count;
  uint32_t recovery_success_count;
} app_modbus_sensor_device_source_t;

typedef struct
{
  uint32_t register_image_generation;
  app_measurement_snapshot_t measurement;
  uint32_t sensor_monitor_schema_revision;
  uint32_t sensor_unavailable_mask;
  uint32_t source_stale_mask;
  uint32_t sensor_recovery_mask;
  uint32_t adxl345_irq_event_count;
  uint32_t adxl345_dropped_sample_lower_bound;
  app_modbus_sensor_device_source_t device[APP_SENSOR_DEVICE_COUNT];
  app_health_state_t health_state;
  uint32_t health_warning_mask;
  uint32_t rs485_error_count;
} app_modbus_register_source_t;

bool app_modbus_register_image_build(
    const app_modbus_register_source_t *source,
    uint16_t *registers,
    size_t register_count);

#endif
