#include "app_sensor_monitor.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint32_t app_sensor_saturating_increment(uint32_t value)
{
  return value == UINT32_MAX ? UINT32_MAX : value + 1U;
}

static uint32_t app_sensor_device_mask(app_sensor_device_t device)
{
  return UINT32_C(1) << (uint32_t)device;
}

static uint32_t app_sensor_source_mask(app_measurement_source_t source)
{
  return UINT32_C(1) << (uint32_t)source;
}

static const app_measurement_metadata_t *app_sensor_metadata(
    const app_measurement_snapshot_t *measurement,
    app_measurement_source_t source)
{
  switch (source)
  {
    case APP_MEASUREMENT_SOURCE_BME280:
      return &measurement->bme280.metadata;
    case APP_MEASUREMENT_SOURCE_VEML7700:
      return &measurement->veml7700.metadata;
    case APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE:
      return &measurement->adxl345_sample.metadata;
    case APP_MEASUREMENT_SOURCE_ADXL345_FEATURE:
      return &measurement->adxl345_feature.metadata;
    case APP_MEASUREMENT_SOURCE_COUNT:
    default:
      return NULL;
  }
}

static app_sensor_fault_class_t app_sensor_bme280_fault(
    const app_bme280_snapshot_t *snapshot)
{
  if ((snapshot->status == BME280_STATUS_WRONG_ID) ||
      (snapshot->status == BME280_STATUS_UNSUPPORTED_BMP280))
  {
    return APP_SENSOR_FAULT_NOT_PRESENT_OR_IDENTITY;
  }
  if ((snapshot->status == BME280_STATUS_CALIBRATION_INVALID) ||
      (snapshot->status == BME280_STATUS_MEASUREMENT_INVALID))
  {
    return APP_SENSOR_FAULT_CONFIGURATION;
  }
  if ((snapshot->status == BME280_STATUS_NVM_TIMEOUT) ||
      (snapshot->status == BME280_STATUS_MEASUREMENT_TIMEOUT))
  {
    return APP_SENSOR_FAULT_TRANSPORT_TIMEOUT;
  }
  if (snapshot->status == BME280_STATUS_RECOVERY_REQUIRED)
  {
    return APP_SENSOR_FAULT_RECOVERY_ACTIVE;
  }
  switch (snapshot->last_transport_result)
  {
    case BME280_TRANSPORT_BUSY:
      return APP_SENSOR_FAULT_TRANSPORT_BUSY;
    case BME280_TRANSPORT_TIMEOUT:
      return APP_SENSOR_FAULT_TRANSPORT_TIMEOUT;
    case BME280_TRANSPORT_IO_ERROR:
      return APP_SENSOR_FAULT_TRANSPORT_IO;
    case BME280_TRANSPORT_INVALID_ARGUMENT:
      return APP_SENSOR_FAULT_CONFIGURATION;
    case BME280_TRANSPORT_OK:
    default:
      break;
  }
  return snapshot->state == BME280_STATE_OFFLINE
             ? APP_SENSOR_FAULT_OFFLINE
             : APP_SENSOR_FAULT_NONE;
}

static app_sensor_fault_class_t app_sensor_veml7700_fault(
    const app_veml7700_snapshot_t *snapshot)
{
  if ((snapshot->status == VEML7700_STATUS_NOT_PRESENT) ||
      (snapshot->last_error_status == VEML7700_STATUS_NOT_PRESENT))
  {
    return APP_SENSOR_FAULT_NOT_PRESENT_OR_IDENTITY;
  }
  if ((snapshot->status == VEML7700_STATUS_CONFIGURATION_MISMATCH) ||
      (snapshot->last_error_status ==
       VEML7700_STATUS_CONFIGURATION_MISMATCH))
  {
    return APP_SENSOR_FAULT_CONFIGURATION;
  }
  if (snapshot->status == VEML7700_STATUS_RECOVERY_REQUIRED)
  {
    return APP_SENSOR_FAULT_RECOVERY_ACTIVE;
  }
  switch (snapshot->last_transport_result)
  {
    case VEML7700_TRANSPORT_NOT_PRESENT:
      return APP_SENSOR_FAULT_NOT_PRESENT_OR_IDENTITY;
    case VEML7700_TRANSPORT_BUSY:
      return APP_SENSOR_FAULT_TRANSPORT_BUSY;
    case VEML7700_TRANSPORT_TIMEOUT:
      return APP_SENSOR_FAULT_TRANSPORT_TIMEOUT;
    case VEML7700_TRANSPORT_IO_ERROR:
      return APP_SENSOR_FAULT_TRANSPORT_IO;
    case VEML7700_TRANSPORT_INVALID_ARGUMENT:
      return APP_SENSOR_FAULT_CONFIGURATION;
    case VEML7700_TRANSPORT_OK:
    default:
      break;
  }
  return snapshot->state == VEML7700_STATE_OFFLINE
             ? APP_SENSOR_FAULT_OFFLINE
             : APP_SENSOR_FAULT_NONE;
}

static app_sensor_fault_class_t app_sensor_adxl345_fault(
    const app_adxl345_snapshot_t *snapshot)
{
  if ((snapshot->status == ADXL345_STATUS_WRONG_ID) ||
      (snapshot->last_error_status == ADXL345_STATUS_WRONG_ID))
  {
    return APP_SENSOR_FAULT_NOT_PRESENT_OR_IDENTITY;
  }
  if ((snapshot->status == ADXL345_STATUS_CONFIGURATION_MISMATCH) ||
      (snapshot->last_error_status ==
       ADXL345_STATUS_CONFIGURATION_MISMATCH))
  {
    return APP_SENSOR_FAULT_CONFIGURATION;
  }
  if ((snapshot->status == ADXL345_STATUS_DATA_READY_STALLED) ||
      (snapshot->last_error_status == ADXL345_STATUS_DATA_READY_STALLED))
  {
    return APP_SENSOR_FAULT_DATA_STALLED;
  }
  if (snapshot->status == ADXL345_STATUS_RECOVERY_REQUIRED)
  {
    return APP_SENSOR_FAULT_RECOVERY_ACTIVE;
  }
  switch (snapshot->last_transport_result)
  {
    case ADXL345_TRANSPORT_BUSY:
      return APP_SENSOR_FAULT_TRANSPORT_BUSY;
    case ADXL345_TRANSPORT_TIMEOUT:
      return APP_SENSOR_FAULT_TRANSPORT_TIMEOUT;
    case ADXL345_TRANSPORT_IO_ERROR:
      return APP_SENSOR_FAULT_TRANSPORT_IO;
    case ADXL345_TRANSPORT_INVALID_ARGUMENT:
      return APP_SENSOR_FAULT_CONFIGURATION;
    case ADXL345_TRANSPORT_OK:
    default:
      break;
  }
  return snapshot->state == ADXL345_STATE_OFFLINE
             ? APP_SENSOR_FAULT_OFFLINE
             : APP_SENSOR_FAULT_NONE;
}

static void app_sensor_update_sample(
    app_sensor_sample_stats_t *stats,
    const app_measurement_metadata_t *metadata)
{
  if (!metadata->value_present ||
      (stats->observed && (stats->last_sequence == metadata->sequence)))
  {
    return;
  }

  if (stats->observed)
  {
    const uint32_t interval =
        metadata->sample_monotonic_ms - stats->last_sample_ms;
    stats->last_interval_ms = interval;
    if (!stats->interval_valid || (interval < stats->minimum_interval_ms))
    {
      stats->minimum_interval_ms = interval;
    }
    if (!stats->interval_valid || (interval > stats->maximum_interval_ms))
    {
      stats->maximum_interval_ms = interval;
    }
    stats->interval_valid = true;
  }

  stats->observed = true;
  stats->accepted_count = app_sensor_saturating_increment(
      stats->accepted_count);
  stats->last_sequence = metadata->sequence;
  stats->last_sample_ms = metadata->sample_monotonic_ms;
}

static void app_sensor_update_device(
    app_sensor_monitor_t *monitor,
    app_sensor_device_t device,
    app_sensor_fault_class_t fault_class,
    uint32_t recovery_request_count,
    uint32_t recovery_success_count,
    bool offline,
    uint32_t now_ms)
{
  const size_t index = (size_t)device;
  app_sensor_device_stats_t *stats = &monitor->snapshot.device[index];
  const bool fault_active = fault_class != APP_SENSOR_FAULT_NONE;

  if (fault_active && !stats->fault_active)
  {
    stats->fault_episode_count = app_sensor_saturating_increment(
        stats->fault_episode_count);
    if (stats->fault_episode_count == 1U)
    {
      stats->first_fault_ms = now_ms;
    }
  }
  if (fault_active)
  {
    stats->last_fault_ms = now_ms;
  }
  stats->fault_active = fault_active;
  stats->fault_class = fault_class;

  if (monitor->observed_recovery_request_count[index] !=
      recovery_request_count)
  {
    monitor->observed_recovery_request_count[index] =
        recovery_request_count;
    monitor->recovery_started_ms[index] = now_ms;
    monitor->recovery_started_valid[index] = true;
  }
  if (monitor->observed_recovery_success_count[index] !=
      recovery_success_count)
  {
    monitor->observed_recovery_success_count[index] =
        recovery_success_count;
    if (monitor->recovery_started_valid[index])
    {
      const uint32_t duration =
          now_ms - monitor->recovery_started_ms[index];
      stats->last_recovery_duration_ms = duration;
      if (!stats->recovery_duration_valid ||
          (duration > stats->maximum_recovery_duration_ms))
      {
        stats->maximum_recovery_duration_ms = duration;
      }
      stats->recovery_duration_valid = true;
    }
    monitor->recovery_started_valid[index] = false;
  }
  if (offline)
  {
    monitor->recovery_started_valid[index] = false;
  }

  stats->recovery_request_count = recovery_request_count;
  stats->recovery_success_count = recovery_success_count;
  stats->recovery_active =
      monitor->recovery_started_valid[index] && !offline;
}

void app_sensor_monitor_initialize(app_sensor_monitor_t *monitor)
{
  if (monitor == NULL)
  {
    return;
  }
  (void)memset(monitor, 0, sizeof(*monitor));
  monitor->snapshot.schema_revision = APP_SENSOR_MONITOR_SCHEMA_REVISION;
}

bool app_sensor_monitor_update(
    app_sensor_monitor_t *monitor,
    const app_measurement_inputs_t *drivers,
    const app_measurement_snapshot_t *measurement,
    uint32_t now_ms)
{
  if ((monitor == NULL) || (drivers == NULL) || (measurement == NULL) ||
      (monitor->snapshot.schema_revision !=
       APP_SENSOR_MONITOR_SCHEMA_REVISION) ||
      (measurement->schema_revision != APP_MEASUREMENT_SCHEMA_REVISION))
  {
    return false;
  }

  monitor->snapshot.evaluated_monotonic_ms = now_ms;
  monitor->snapshot.unavailable_device_mask = 0U;
  monitor->snapshot.stale_source_mask = 0U;
  monitor->snapshot.recovery_device_mask = 0U;

  for (size_t index = 0U; index < APP_MEASUREMENT_SOURCE_COUNT; ++index)
  {
    const app_measurement_source_t source =
        (app_measurement_source_t)index;
    const app_measurement_metadata_t *metadata =
        app_sensor_metadata(measurement, source);
    app_sensor_update_sample(&monitor->snapshot.sample[index], metadata);
    if (metadata->state == APP_MEASUREMENT_STATE_STALE)
    {
      monitor->snapshot.stale_source_mask |=
          app_sensor_source_mask(source);
    }
  }

  const bool bme_offline =
      drivers->bme280.state == BME280_STATE_OFFLINE;
  const bool veml_offline =
      drivers->veml7700.state == VEML7700_STATE_OFFLINE;
  const bool adxl_offline =
      drivers->adxl345.state == ADXL345_STATE_OFFLINE;
  app_sensor_update_device(
      monitor,
      APP_SENSOR_DEVICE_BME280,
      app_sensor_bme280_fault(&drivers->bme280),
      drivers->bme280.recovery_request_count,
      drivers->bme280.recovery_success_count,
      bme_offline,
      now_ms);
  app_sensor_update_device(
      monitor,
      APP_SENSOR_DEVICE_VEML7700,
      app_sensor_veml7700_fault(&drivers->veml7700),
      drivers->veml7700.recovery_request_count,
      drivers->veml7700.recovery_success_count,
      veml_offline,
      now_ms);
  app_sensor_update_device(
      monitor,
      APP_SENSOR_DEVICE_ADXL345,
      app_sensor_adxl345_fault(&drivers->adxl345),
      drivers->adxl345.recovery_request_count,
      drivers->adxl345.recovery_success_count,
      adxl_offline,
      now_ms);

  const bool offline[APP_SENSOR_DEVICE_COUNT] = {
      bme_offline, veml_offline, adxl_offline};
  for (size_t index = 0U; index < APP_SENSOR_DEVICE_COUNT; ++index)
  {
    const app_sensor_device_t device = (app_sensor_device_t)index;
    if (offline[index])
    {
      monitor->snapshot.unavailable_device_mask |=
          app_sensor_device_mask(device);
    }
    if (monitor->snapshot.device[index].recovery_active)
    {
      monitor->snapshot.recovery_device_mask |=
          app_sensor_device_mask(device);
    }
  }

  monitor->snapshot.adxl345_irq_event_count =
      drivers->adxl345.irq_event_count;
  monitor->snapshot.adxl345_dropped_sample_lower_bound =
      drivers->adxl345.dropped_sample_lower_bound;
  return true;
}

bool app_sensor_monitor_get_snapshot(
    const app_sensor_monitor_t *monitor,
    app_sensor_monitor_snapshot_t *snapshot)
{
  if ((monitor == NULL) || (snapshot == NULL) ||
      (monitor->snapshot.schema_revision !=
       APP_SENSOR_MONITOR_SCHEMA_REVISION))
  {
    return false;
  }
  *snapshot = monitor->snapshot;
  return true;
}
