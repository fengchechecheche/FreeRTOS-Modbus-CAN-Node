#ifndef APP_SENSOR_MONITOR_H
#define APP_SENSOR_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#include "app_measurement.h"

#define APP_SENSOR_MONITOR_SCHEMA_REVISION UINT32_C(1)

typedef enum
{
  APP_SENSOR_DEVICE_BME280 = 0,
  APP_SENSOR_DEVICE_VEML7700,
  APP_SENSOR_DEVICE_ADXL345,
  APP_SENSOR_DEVICE_COUNT
} app_sensor_device_t;

typedef enum
{
  APP_SENSOR_FAULT_NONE = 0,
  APP_SENSOR_FAULT_NOT_PRESENT_OR_IDENTITY,
  APP_SENSOR_FAULT_TRANSPORT_BUSY,
  APP_SENSOR_FAULT_TRANSPORT_TIMEOUT,
  APP_SENSOR_FAULT_TRANSPORT_IO,
  APP_SENSOR_FAULT_CONFIGURATION,
  APP_SENSOR_FAULT_DATA_STALLED,
  APP_SENSOR_FAULT_RECOVERY_ACTIVE,
  APP_SENSOR_FAULT_OFFLINE
} app_sensor_fault_class_t;

typedef struct
{
  uint32_t accepted_count;
  uint32_t last_sequence;
  uint32_t last_sample_ms;
  uint32_t last_interval_ms;
  uint32_t minimum_interval_ms;
  uint32_t maximum_interval_ms;
  bool observed;
  bool interval_valid;
} app_sensor_sample_stats_t;

typedef struct
{
  app_sensor_fault_class_t fault_class;
  uint32_t fault_episode_count;
  uint32_t first_fault_ms;
  uint32_t last_fault_ms;
  uint32_t recovery_request_count;
  uint32_t recovery_success_count;
  uint32_t last_recovery_duration_ms;
  uint32_t maximum_recovery_duration_ms;
  bool fault_active;
  bool recovery_active;
  bool recovery_duration_valid;
} app_sensor_device_stats_t;

typedef struct
{
  uint32_t schema_revision;
  uint32_t evaluated_monotonic_ms;
  app_sensor_sample_stats_t sample[APP_MEASUREMENT_SOURCE_COUNT];
  app_sensor_device_stats_t device[APP_SENSOR_DEVICE_COUNT];
  uint32_t unavailable_device_mask;
  uint32_t stale_source_mask;
  uint32_t recovery_device_mask;
  uint32_t adxl345_irq_event_count;
  uint32_t adxl345_dropped_sample_lower_bound;
} app_sensor_monitor_snapshot_t;

typedef struct
{
  app_sensor_monitor_snapshot_t snapshot;
  uint32_t observed_recovery_request_count[APP_SENSOR_DEVICE_COUNT];
  uint32_t observed_recovery_success_count[APP_SENSOR_DEVICE_COUNT];
  uint32_t recovery_started_ms[APP_SENSOR_DEVICE_COUNT];
  bool recovery_started_valid[APP_SENSOR_DEVICE_COUNT];
} app_sensor_monitor_t;

void app_sensor_monitor_initialize(app_sensor_monitor_t *monitor);
bool app_sensor_monitor_update(
    app_sensor_monitor_t *monitor,
    const app_measurement_inputs_t *drivers,
    const app_measurement_snapshot_t *measurement,
    uint32_t now_ms);
bool app_sensor_monitor_get_snapshot(
    const app_sensor_monitor_t *monitor,
    app_sensor_monitor_snapshot_t *snapshot);

#endif
