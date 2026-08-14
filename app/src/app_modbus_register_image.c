#include "app_modbus_register_image.h"

#include <string.h>

#define APP_MODBUS_METADATA_BASE (52U)
#define APP_MODBUS_METADATA_STRIDE (10U)
#define APP_MODBUS_METADATA_FLAG_VALUE_PRESENT (UINT16_C(1) << 0U)
#define APP_MODBUS_METADATA_FLAG_VALUE_RETAINED (UINT16_C(1) << 1U)
#define APP_MODBUS_KNOWN_QUALITY_MASK UINT32_C(0x000000ff)

static void app_modbus_write_u32(uint16_t *registers,
                                 size_t address,
                                 uint32_t value)
{
  registers[address] = (uint16_t)(value >> 16U);
  registers[address + 1U] = (uint16_t)(value & UINT32_C(0x0000ffff));
}

static bool app_modbus_measurement_state_wire(
    app_measurement_state_t state,
    uint16_t *wire)
{
  if (wire == NULL)
  {
    return false;
  }
  switch (state)
  {
    case APP_MEASUREMENT_STATE_INVALID:
      *wire = 0U;
      return true;
    case APP_MEASUREMENT_STATE_FRESH:
      *wire = 1U;
      return true;
    case APP_MEASUREMENT_STATE_STALE:
      *wire = 2U;
      return true;
    case APP_MEASUREMENT_STATE_OFFLINE:
      *wire = 3U;
      return true;
    default:
      return false;
  }
}

static bool app_modbus_sensor_fault_wire(
    app_sensor_fault_class_t fault,
    uint16_t *wire)
{
  if (wire == NULL)
  {
    return false;
  }
  switch (fault)
  {
    case APP_SENSOR_FAULT_NONE:
      *wire = 0U;
      return true;
    case APP_SENSOR_FAULT_NOT_PRESENT_OR_IDENTITY:
      *wire = 1U;
      return true;
    case APP_SENSOR_FAULT_TRANSPORT_BUSY:
      *wire = 2U;
      return true;
    case APP_SENSOR_FAULT_TRANSPORT_TIMEOUT:
      *wire = 3U;
      return true;
    case APP_SENSOR_FAULT_TRANSPORT_IO:
      *wire = 4U;
      return true;
    case APP_SENSOR_FAULT_CONFIGURATION:
      *wire = 5U;
      return true;
    case APP_SENSOR_FAULT_DATA_STALLED:
      *wire = 6U;
      return true;
    case APP_SENSOR_FAULT_RECOVERY_ACTIVE:
      *wire = 7U;
      return true;
    case APP_SENSOR_FAULT_OFFLINE:
      *wire = 8U;
      return true;
    default:
      return false;
  }
}

static bool app_modbus_health_state_wire(app_health_state_t state,
                                         uint16_t *wire)
{
  if (wire == NULL)
  {
    return false;
  }
  switch (state)
  {
    case APP_HEALTH_BOOTSTRAP:
      *wire = 0U;
      return true;
    case APP_HEALTH_SERVICEABLE:
      *wire = 1U;
      return true;
    case APP_HEALTH_DEGRADED:
      *wire = 2U;
      return true;
    case APP_HEALTH_RECOVERY_REQUIRED:
      *wire = 3U;
      return true;
    case APP_HEALTH_RESET_REQUIRED:
      *wire = 4U;
      return true;
    case APP_HEALTH_RESET_LOOP_LATCHED:
      *wire = 5U;
      return true;
    default:
      return false;
  }
}

static const app_measurement_metadata_t *app_modbus_metadata(
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
    default:
      return NULL;
  }
}

static bool app_modbus_write_metadata(
    const app_measurement_metadata_t *metadata,
    size_t address,
    uint16_t *registers)
{
  uint16_t state = 0U;
  if ((metadata == NULL) ||
      !app_modbus_measurement_state_wire(metadata->state, &state))
  {
    return false;
  }

  uint16_t flags = 0U;
  if (metadata->value_present)
  {
    flags |= APP_MODBUS_METADATA_FLAG_VALUE_PRESENT;
  }
  if (metadata->value_is_retained)
  {
    flags |= APP_MODBUS_METADATA_FLAG_VALUE_RETAINED;
  }
  registers[address] = state;
  registers[address + 1U] = flags;
  app_modbus_write_u32(registers,
                       address + 2U,
                       metadata->quality_flags &
                           APP_MODBUS_KNOWN_QUALITY_MASK);
  app_modbus_write_u32(registers, address + 4U, metadata->sequence);
  app_modbus_write_u32(
      registers, address + 6U, metadata->sample_monotonic_ms);
  app_modbus_write_u32(registers, address + 8U, metadata->age_ms);
  return true;
}

static void app_modbus_write_measurements(
    const app_measurement_snapshot_t *measurement,
    uint16_t *registers)
{
  if (measurement->bme280.metadata.value_present)
  {
    app_modbus_write_u32(
        registers, 18U, (uint32_t)measurement->bme280.temperature_centi_c);
    app_modbus_write_u32(registers, 20U, measurement->bme280.pressure_pa);
    app_modbus_write_u32(
        registers, 22U, measurement->bme280.humidity_milli_pct);
  }
  if (measurement->veml7700.metadata.value_present)
  {
    app_modbus_write_u32(
        registers, 24U, measurement->veml7700.illuminance_millilux);
  }
  if (measurement->adxl345_sample.metadata.value_present)
  {
    for (size_t axis = 0U; axis < ADXL345_AXIS_COUNT; ++axis)
    {
      app_modbus_write_u32(
          registers,
          26U + (axis * 2U),
          (uint32_t)measurement->adxl345_sample.acceleration_millig[axis]);
    }
  }
  if (measurement->adxl345_feature.metadata.value_present)
  {
    for (size_t axis = 0U; axis < ADXL345_AXIS_COUNT; ++axis)
    {
      app_modbus_write_u32(
          registers,
          32U + (axis * 2U),
          (uint32_t)measurement->adxl345_feature.mean_millig[axis]);
      app_modbus_write_u32(
          registers,
          38U + (axis * 2U),
          measurement->adxl345_feature.rms_millig[axis]);
      app_modbus_write_u32(
          registers,
          44U + (axis * 2U),
          measurement->adxl345_feature.peak_abs_millig[axis]);
    }
    app_modbus_write_u32(
        registers,
        50U,
        measurement->adxl345_feature.resultant_rms_millig);
  }
}

static bool app_modbus_write_device_diagnostics(
    const app_modbus_register_source_t *source,
    uint16_t *registers)
{
  static const size_t address[APP_SENSOR_DEVICE_COUNT] = {96U, 103U, 110U};
  for (size_t index = 0U; index < APP_SENSOR_DEVICE_COUNT; ++index)
  {
    uint16_t fault = 0U;
    if (!app_modbus_sensor_fault_wire(source->device[index].fault_class,
                                      &fault))
    {
      return false;
    }
    registers[address[index]] = fault;
    app_modbus_write_u32(registers,
                         address[index] + 1U,
                         source->device[index].fault_episode_count);
    app_modbus_write_u32(registers,
                         address[index] + 3U,
                         source->device[index].recovery_request_count);
    app_modbus_write_u32(registers,
                         address[index] + 5U,
                         source->device[index].recovery_success_count);
  }
  return true;
}

bool app_modbus_register_image_build(
    const app_modbus_register_source_t *source,
    uint16_t *registers,
    size_t register_count)
{
  if ((source == NULL) || (registers == NULL) ||
      (register_count < P5_MODBUS_SERVER_INPUT_REGISTER_COUNT) ||
      (source->measurement.schema_revision !=
       APP_MEASUREMENT_SCHEMA_REVISION) ||
      (source->sensor_monitor_schema_revision !=
       APP_SENSOR_MONITOR_SCHEMA_REVISION))
  {
    return false;
  }

  (void)memset(registers,
               0,
               P5_MODBUS_SERVER_INPUT_REGISTER_COUNT * sizeof(uint16_t));
  registers[0] = APP_MODBUS_DEVICE_SIGNATURE;
  registers[1] = APP_MODBUS_REGISTER_MAP_REVISION;
  registers[2] = (uint16_t)source->measurement.schema_revision;
  registers[3] = (uint16_t)source->sensor_monitor_schema_revision;
  registers[4] = APP_MODBUS_FIRMWARE_VERSION_MAJOR;
  registers[5] = APP_MODBUS_FIRMWARE_VERSION_MINOR;
  registers[6] = APP_MODBUS_FIRMWARE_VERSION_PATCH;
  registers[7] = APP_MODBUS_FIRMWARE_VERSION_FLAGS;
  app_modbus_write_u32(
      registers, 8U, source->register_image_generation);
  app_modbus_write_u32(
      registers, 10U, source->measurement.evaluated_monotonic_ms);

  uint16_t present_mask = 0U;
  uint16_t retained_mask = 0U;
  for (size_t index = 0U; index < APP_MEASUREMENT_SOURCE_COUNT; ++index)
  {
    const app_measurement_metadata_t *metadata = app_modbus_metadata(
        &source->measurement, (app_measurement_source_t)index);
    if (metadata == NULL)
    {
      return false;
    }
    if (metadata->value_present)
    {
      present_mask |= (uint16_t)(UINT16_C(1) << index);
    }
    if (metadata->value_is_retained)
    {
      retained_mask |= (uint16_t)(UINT16_C(1) << index);
    }
    if (!app_modbus_write_metadata(
            metadata,
            APP_MODBUS_METADATA_BASE +
                (index * APP_MODBUS_METADATA_STRIDE),
            registers))
    {
      return false;
    }
  }
  registers[12] = present_mask;
  registers[13] = retained_mask;
  registers[14] = (uint16_t)(source->source_stale_mask & UINT32_C(0x000f));
  registers[15] =
      (uint16_t)(source->sensor_unavailable_mask & UINT32_C(0x0007));
  registers[16] =
      (uint16_t)(source->sensor_recovery_mask & UINT32_C(0x0007));
  registers[17] = (uint16_t)(
      ((uint16_t)APP_MEASUREMENT_SOURCE_COUNT << 8U) |
      (uint16_t)APP_SENSOR_DEVICE_COUNT);

  app_modbus_write_measurements(&source->measurement, registers);
  app_modbus_write_u32(
      registers, 92U, source->adxl345_irq_event_count);
  app_modbus_write_u32(
      registers, 94U, source->adxl345_dropped_sample_lower_bound);
  if (!app_modbus_write_device_diagnostics(source, registers))
  {
    return false;
  }

  if (!app_modbus_health_state_wire(source->health_state, &registers[117]))
  {
    return false;
  }
  app_modbus_write_u32(registers, 118U, source->health_warning_mask);
  app_modbus_write_u32(registers, 120U, source->rs485_error_count);
  return true;
}
