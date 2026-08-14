#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_modbus_register_image.h"

#define CHECK(condition)                                                     \
  do                                                                         \
  {                                                                          \
    if (!(condition))                                                        \
    {                                                                        \
      (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",                 \
                    __FILE__, __LINE__, #condition);                         \
      return false;                                                          \
    }                                                                        \
  } while (0)

static uint32_t read_u32(const uint16_t *registers, size_t address)
{
  return ((uint32_t)registers[address] << 16U) |
         (uint32_t)registers[address + 1U];
}

static app_measurement_metadata_t metadata(
    app_measurement_source_t source,
    app_measurement_state_t state,
    uint32_t sequence,
    bool present,
    bool retained)
{
  const app_measurement_metadata_t value = {
      .source = source,
      .state = state,
      .sequence = sequence,
      .sample_monotonic_ms = UINT32_C(0xfffffff0) + sequence,
      .age_ms = UINT32_C(0x20) + sequence,
      .quality_flags = APP_MEASUREMENT_QUALITY_RANGE_WARNING |
                       APP_MEASUREMENT_QUALITY_TRANSPORT_ERROR |
                       UINT32_C(0x80000000),
      .value_present = present,
      .value_is_retained = retained,
  };
  return value;
}

static void source_initialize(app_modbus_register_source_t *source)
{
  (void)memset(source, 0, sizeof(*source));
  source->register_image_generation = UINT32_C(0x11223344);
  source->measurement.schema_revision = APP_MEASUREMENT_SCHEMA_REVISION;
  source->measurement.evaluated_monotonic_ms = UINT32_C(0xaabbccdd);
  source->measurement.bme280.metadata = metadata(
      APP_MEASUREMENT_SOURCE_BME280,
      APP_MEASUREMENT_STATE_STALE,
      1U,
      true,
      true);
  source->measurement.veml7700.metadata = metadata(
      APP_MEASUREMENT_SOURCE_VEML7700,
      APP_MEASUREMENT_STATE_INVALID,
      2U,
      false,
      false);
  source->measurement.adxl345_sample.metadata = metadata(
      APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE,
      APP_MEASUREMENT_STATE_FRESH,
      3U,
      true,
      false);
  source->measurement.adxl345_feature.metadata = metadata(
      APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
      APP_MEASUREMENT_STATE_OFFLINE,
      4U,
      true,
      true);

  source->measurement.bme280.temperature_centi_c = -12345;
  source->measurement.bme280.pressure_pa = UINT32_C(0xffffffff);
  source->measurement.bme280.humidity_milli_pct = UINT32_C(45678);
  source->measurement.veml7700.illuminance_millilux = UINT32_C(0xdeadbeef);
  source->measurement.adxl345_sample.acceleration_millig[0] = -1;
  source->measurement.adxl345_sample.acceleration_millig[1] = 2;
  source->measurement.adxl345_sample.acceleration_millig[2] = -3;
  source->measurement.adxl345_feature.mean_millig[0] = -100;
  source->measurement.adxl345_feature.mean_millig[1] = 200;
  source->measurement.adxl345_feature.mean_millig[2] = -300;
  source->measurement.adxl345_feature.rms_millig[0] = 400U;
  source->measurement.adxl345_feature.rms_millig[1] = 500U;
  source->measurement.adxl345_feature.rms_millig[2] = 600U;
  source->measurement.adxl345_feature.peak_abs_millig[0] = 700U;
  source->measurement.adxl345_feature.peak_abs_millig[1] = 800U;
  source->measurement.adxl345_feature.peak_abs_millig[2] = 900U;
  source->measurement.adxl345_feature.resultant_rms_millig = 1000U;

  source->sensor_monitor_schema_revision =
      APP_SENSOR_MONITOR_SCHEMA_REVISION;
  source->sensor_unavailable_mask = 5U;
  source->source_stale_mask = 9U;
  source->sensor_recovery_mask = 2U;
  source->adxl345_irq_event_count = UINT32_C(0x01020304);
  source->adxl345_dropped_sample_lower_bound = UINT32_C(0x05060708);
  source->device[0] = (app_modbus_sensor_device_source_t){
      APP_SENSOR_FAULT_NONE, 1U, 2U, 3U};
  source->device[1] = (app_modbus_sensor_device_source_t){
      APP_SENSOR_FAULT_TRANSPORT_TIMEOUT, 4U, 5U, 6U};
  source->device[2] = (app_modbus_sensor_device_source_t){
      APP_SENSOR_FAULT_OFFLINE, 7U, 8U, 9U};
  source->health_state = APP_HEALTH_DEGRADED;
  source->health_warning_mask = UINT32_C(0x89abcdef);
  source->rs485_error_count = UINT32_C(0x76543210);
}

static bool test_complete_map(void)
{
  app_modbus_register_source_t source;
  source_initialize(&source);
  uint16_t registers[P5_MODBUS_SERVER_INPUT_REGISTER_COUNT];
  CHECK(app_modbus_register_image_build(
      &source, registers, P5_MODBUS_SERVER_INPUT_REGISTER_COUNT));

  CHECK(registers[0] == UINT16_C(0x5035));
  CHECK(registers[1] == 1U);
  CHECK(registers[2] == APP_MEASUREMENT_SCHEMA_REVISION);
  CHECK(registers[3] == APP_SENSOR_MONITOR_SCHEMA_REVISION);
  CHECK(registers[4] == 0U && registers[5] == 0U &&
        registers[6] == 0U && registers[7] == 0U);
  CHECK(read_u32(registers, 8U) == UINT32_C(0x11223344));
  CHECK(read_u32(registers, 10U) == UINT32_C(0xaabbccdd));
  CHECK(registers[12] == UINT16_C(0x000d));
  CHECK(registers[13] == UINT16_C(0x0009));
  CHECK(registers[14] == 9U);
  CHECK(registers[15] == 5U);
  CHECK(registers[16] == 2U);
  CHECK(registers[17] == UINT16_C(0x0403));

  CHECK(read_u32(registers, 18U) == (uint32_t)(int32_t)-12345);
  CHECK(read_u32(registers, 20U) == UINT32_C(0xffffffff));
  CHECK(read_u32(registers, 22U) == UINT32_C(45678));
  CHECK(read_u32(registers, 24U) == 0U);
  CHECK(read_u32(registers, 26U) == UINT32_C(0xffffffff));
  CHECK(read_u32(registers, 28U) == 2U);
  CHECK(read_u32(registers, 30U) == (uint32_t)(int32_t)-3);
  CHECK(read_u32(registers, 32U) == (uint32_t)(int32_t)-100);
  CHECK(read_u32(registers, 38U) == 400U);
  CHECK(read_u32(registers, 44U) == 700U);
  CHECK(read_u32(registers, 50U) == 1000U);

  CHECK(registers[52] == 2U);
  CHECK(registers[53] == 3U);
  CHECK(read_u32(registers, 54U) ==
        (APP_MEASUREMENT_QUALITY_RANGE_WARNING |
         APP_MEASUREMENT_QUALITY_TRANSPORT_ERROR));
  CHECK(read_u32(registers, 56U) == 1U);
  CHECK(read_u32(registers, 58U) == UINT32_C(0xfffffff1));
  CHECK(read_u32(registers, 60U) == UINT32_C(0x21));
  CHECK(registers[62] == 0U);
  CHECK(registers[63] == 0U);
  CHECK(registers[72] == 1U);
  CHECK(registers[73] == 1U);
  CHECK(registers[82] == 3U);
  CHECK(registers[83] == 3U);

  CHECK(read_u32(registers, 92U) == UINT32_C(0x01020304));
  CHECK(read_u32(registers, 94U) == UINT32_C(0x05060708));
  CHECK(registers[96] == 0U);
  CHECK(read_u32(registers, 97U) == 1U);
  CHECK(read_u32(registers, 99U) == 2U);
  CHECK(read_u32(registers, 101U) == 3U);
  CHECK(registers[103] == 3U);
  CHECK(read_u32(registers, 104U) == 4U);
  CHECK(registers[110] == 8U);
  CHECK(read_u32(registers, 115U) == 9U);

  CHECK(registers[117] == 2U);
  CHECK(read_u32(registers, 118U) == UINT32_C(0x89abcdef));
  CHECK(read_u32(registers, 120U) == UINT32_C(0x76543210));
  return true;
}

static bool test_validation(void)
{
  app_modbus_register_source_t source;
  source_initialize(&source);
  uint16_t registers[P5_MODBUS_SERVER_INPUT_REGISTER_COUNT];
  CHECK(!app_modbus_register_image_build(
      NULL, registers, P5_MODBUS_SERVER_INPUT_REGISTER_COUNT));
  CHECK(!app_modbus_register_image_build(
      &source, NULL, P5_MODBUS_SERVER_INPUT_REGISTER_COUNT));
  CHECK(!app_modbus_register_image_build(
      &source, registers, P5_MODBUS_SERVER_INPUT_REGISTER_COUNT - 1U));

  source.measurement.schema_revision = 2U;
  CHECK(!app_modbus_register_image_build(
      &source, registers, P5_MODBUS_SERVER_INPUT_REGISTER_COUNT));
  source_initialize(&source);
  source.measurement.bme280.metadata.state =
      (app_measurement_state_t)99;
  CHECK(!app_modbus_register_image_build(
      &source, registers, P5_MODBUS_SERVER_INPUT_REGISTER_COUNT));
  source_initialize(&source);
  source.device[0].fault_class = (app_sensor_fault_class_t)99;
  CHECK(!app_modbus_register_image_build(
      &source, registers, P5_MODBUS_SERVER_INPUT_REGISTER_COUNT));
  source_initialize(&source);
  source.health_state = (app_health_state_t)99;
  CHECK(!app_modbus_register_image_build(
      &source, registers, P5_MODBUS_SERVER_INPUT_REGISTER_COUNT));
  return true;
}

int main(void)
{
  CHECK(test_complete_map());
  CHECK(test_validation());
  (void)puts("modbus register image tests passed");
  return 0;
}
