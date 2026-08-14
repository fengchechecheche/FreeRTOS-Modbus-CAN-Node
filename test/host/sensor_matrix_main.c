#include "app_health_policy.h"
#include "app_sensor_monitor.h"
#include "app_transport_policy.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)          \
  do                              \
  {                               \
    if (!(condition))             \
    {                             \
      return EXIT_FAILURE;        \
    }                             \
  } while (false)

typedef struct
{
  uint8_t chip_id;
  uint8_t status_value;
  uint8_t calibration_1[BME280_CALIBRATION_1_LENGTH];
  uint8_t calibration_2[BME280_CALIBRATION_2_LENGTH];
  uint8_t raw[BME280_RAW_DATA_LENGTH];
  bme280_transport_result_t forced_result;
  bool force_result;
  uint32_t transaction_count;
} bme_bus_t;

typedef struct
{
  uint16_t config_word;
  uint16_t raw_als;
  veml7700_transport_result_t forced_result;
  bool force_result;
  bool ignore_config_write;
  uint32_t transaction_count;
} veml_bus_t;

typedef struct
{
  uint8_t registers[256];
  uint8_t sample_data[ADXL345_DATA_LENGTH];
  adxl345_transport_result_t forced_result;
  bool force_result;
  uint32_t transaction_count;
  uint32_t data_read_count;
} adxl_bus_t;

typedef struct
{
  bme280_t bme;
  veml7700_t veml;
  adxl345_t adxl;
  bme_bus_t bme_bus;
  veml_bus_t veml_bus;
  adxl_bus_t adxl_bus;
  app_measurement_inputs_t inputs;
  app_measurement_model_t measurement_model;
  app_measurement_snapshot_t measurement;
  app_sensor_monitor_t monitor;
  app_sensor_monitor_snapshot_t monitor_snapshot;
} combined_system_t;

static void put_u16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)(value & UINT16_C(0x00ff));
  data[1] = (uint8_t)(value >> 8U);
}

static void put_s16(uint8_t *data, int16_t value)
{
  put_u16(data, (uint16_t)value);
}

static void bme_make_reference(bme_bus_t *bus)
{
  (void)memset(bus, 0, sizeof(*bus));
  bus->chip_id = BME280_CHIP_ID;
  put_u16(&bus->calibration_1[0], UINT16_C(27504));
  put_s16(&bus->calibration_1[2], INT16_C(26435));
  put_s16(&bus->calibration_1[4], INT16_C(-1000));
  put_u16(&bus->calibration_1[6], UINT16_C(36477));
  put_s16(&bus->calibration_1[8], INT16_C(-10685));
  put_s16(&bus->calibration_1[10], INT16_C(3024));
  put_s16(&bus->calibration_1[12], INT16_C(2855));
  put_s16(&bus->calibration_1[14], INT16_C(140));
  put_s16(&bus->calibration_1[16], INT16_C(-7));
  put_s16(&bus->calibration_1[18], INT16_C(15500));
  put_s16(&bus->calibration_1[20], INT16_C(-14600));
  put_s16(&bus->calibration_1[22], INT16_C(6000));
  bus->calibration_1[25] = UINT8_C(75);
  put_s16(&bus->calibration_2[0], INT16_C(362));
  bus->calibration_2[3] = UINT8_C(0x14);
  bus->calibration_2[4] = UINT8_C(0x2e);
  bus->calibration_2[5] = UINT8_C(0x03);
  bus->calibration_2[6] = UINT8_C(30);

  const uint32_t pressure = UINT32_C(415148);
  const uint32_t temperature = UINT32_C(519888);
  const uint16_t humidity = UINT16_C(35000);
  bus->raw[0] = (uint8_t)(pressure >> 12U);
  bus->raw[1] = (uint8_t)(pressure >> 4U);
  bus->raw[2] = (uint8_t)((pressure & UINT32_C(0x0f)) << 4U);
  bus->raw[3] = (uint8_t)(temperature >> 12U);
  bus->raw[4] = (uint8_t)(temperature >> 4U);
  bus->raw[5] = (uint8_t)((temperature & UINT32_C(0x0f)) << 4U);
  bus->raw[6] = (uint8_t)(humidity >> 8U);
  bus->raw[7] = (uint8_t)humidity;
}

static bme280_transport_result_t bme_read(void *context,
                                          uint8_t register_address,
                                          uint8_t *data,
                                          size_t length)
{
  bme_bus_t *bus = context;
  ++bus->transaction_count;
  if (bus->force_result)
  {
    return bus->forced_result;
  }
  if ((register_address == BME280_CHIP_ID_REGISTER) && (length == 1U))
  {
    data[0] = bus->chip_id;
  }
  else if ((register_address == BME280_STATUS_REGISTER) && (length == 1U))
  {
    data[0] = bus->status_value;
  }
  else if ((register_address == BME280_CALIBRATION_1_REGISTER) &&
           (length == BME280_CALIBRATION_1_LENGTH))
  {
    (void)memcpy(data, bus->calibration_1, length);
  }
  else if ((register_address == BME280_CALIBRATION_2_REGISTER) &&
           (length == BME280_CALIBRATION_2_LENGTH))
  {
    (void)memcpy(data, bus->calibration_2, length);
  }
  else if ((register_address == BME280_RAW_DATA_REGISTER) &&
           (length == BME280_RAW_DATA_LENGTH))
  {
    (void)memcpy(data, bus->raw, length);
  }
  else
  {
    return BME280_TRANSPORT_INVALID_ARGUMENT;
  }
  return BME280_TRANSPORT_OK;
}

static bme280_transport_result_t bme_write(void *context,
                                           uint8_t register_address,
                                           uint8_t value)
{
  bme_bus_t *bus = context;
  (void)register_address;
  (void)value;
  ++bus->transaction_count;
  return bus->force_result ? bus->forced_result : BME280_TRANSPORT_OK;
}

static veml7700_transport_result_t veml_read(void *context,
                                             uint8_t register_address,
                                             uint8_t *data,
                                             size_t length)
{
  veml_bus_t *bus = context;
  ++bus->transaction_count;
  if (bus->force_result)
  {
    return bus->forced_result;
  }
  if ((data == NULL) || (length != VEML7700_WORD_LENGTH))
  {
    return VEML7700_TRANSPORT_INVALID_ARGUMENT;
  }
  if (register_address == VEML7700_CONFIG_REGISTER)
  {
    (void)veml7700_encode_word(bus->config_word, data);
  }
  else if (register_address == VEML7700_ALS_REGISTER)
  {
    (void)veml7700_encode_word(bus->raw_als, data);
  }
  else
  {
    return VEML7700_TRANSPORT_INVALID_ARGUMENT;
  }
  return VEML7700_TRANSPORT_OK;
}

static veml7700_transport_result_t veml_write(void *context,
                                              uint8_t register_address,
                                              const uint8_t *data,
                                              size_t length)
{
  veml_bus_t *bus = context;
  ++bus->transaction_count;
  if (bus->force_result)
  {
    return bus->forced_result;
  }
  if ((register_address != VEML7700_CONFIG_REGISTER) || (data == NULL) ||
      (length != VEML7700_WORD_LENGTH))
  {
    return VEML7700_TRANSPORT_INVALID_ARGUMENT;
  }
  if (!bus->ignore_config_write)
  {
    bus->config_word = veml7700_decode_word(data);
  }
  return VEML7700_TRANSPORT_OK;
}

static bool veml_recovery_unavailable(void *context)
{
  (void)context;
  return false;
}

static adxl345_transport_result_t adxl_read(void *context,
                                            uint8_t register_address,
                                            uint8_t *data,
                                            size_t length)
{
  adxl_bus_t *bus = context;
  ++bus->transaction_count;
  if (bus->force_result)
  {
    return bus->forced_result;
  }
  if ((register_address == ADXL345_DATAX0_REGISTER) &&
      (length == ADXL345_DATA_LENGTH))
  {
    (void)memcpy(data, bus->sample_data, length);
    ++bus->data_read_count;
  }
  else
  {
    for (size_t index = 0U; index < length; ++index)
    {
      data[index] = bus->registers[(uint8_t)(register_address + index)];
    }
  }
  return ADXL345_TRANSPORT_OK;
}

static adxl345_transport_result_t adxl_write(void *context,
                                             uint8_t register_address,
                                             uint8_t value)
{
  adxl_bus_t *bus = context;
  ++bus->transaction_count;
  if (bus->force_result)
  {
    return bus->forced_result;
  }
  bus->registers[register_address] = value;
  return ADXL345_TRANSPORT_OK;
}

static void adxl_set_sample(adxl_bus_t *bus,
                            int16_t x,
                            int16_t y,
                            int16_t z)
{
  const int16_t values[ADXL345_AXIS_COUNT] = {x, y, z};
  for (size_t axis = 0U; axis < ADXL345_AXIS_COUNT; ++axis)
  {
    const uint16_t raw = (uint16_t)values[axis];
    bus->sample_data[axis * 2U] = (uint8_t)(raw & UINT16_C(0x00ff));
    bus->sample_data[axis * 2U + 1U] = (uint8_t)(raw >> 8U);
  }
}

static bool combined_initialize(combined_system_t *system)
{
  (void)memset(system, 0, sizeof(*system));
  bme_make_reference(&system->bme_bus);
  system->veml_bus.config_word = UINT16_C(0x0001);
  system->veml_bus.raw_als = UINT16_C(1000);
  system->adxl_bus.registers[ADXL345_DEVID_REGISTER] =
      ADXL345_DEVID_VALUE;
  adxl_set_sample(&system->adxl_bus, 0, 0, 256);

  const bme280_ops_t bme_ops = {
      .context = &system->bme_bus,
      .read = bme_read,
      .write = bme_write,
  };
  const veml7700_ops_t veml_ops = {
      .context = &system->veml_bus,
      .read = veml_read,
      .write = veml_write,
      .request_recovery = veml_recovery_unavailable,
  };
  const adxl345_ops_t adxl_ops = {
      .context = &system->adxl_bus,
      .read = adxl_read,
      .write = adxl_write,
  };
  const adxl345_config_t adxl_config = {
      .bias_lsb = {0, 0, 0},
      .bias_calibrated = false,
  };
  if (!bme280_initialize(
          &system->bme, &bme_ops, BME280_DEFAULT_SAMPLE_PERIOD_MS) ||
      !veml7700_initialize(
          &system->veml, &veml_ops, VEML7700_DEFAULT_SAMPLE_PERIOD_MS) ||
      !adxl345_initialize(&system->adxl, &adxl_ops, &adxl_config))
  {
    return false;
  }
  app_measurement_model_initialize(&system->measurement_model);
  app_sensor_monitor_initialize(&system->monitor);
  return true;
}

static void combined_project_driver_snapshots(combined_system_t *system)
{
  app_bme280_snapshot_t *bme = &system->inputs.bme280;
  bme->state = system->bme.state;
  bme->status = system->bme.status;
  bme->last_transport_result = system->bme.last_transport_result;
  bme->sample = system->bme.sample;
  bme->transaction_count = system->bme.transaction_count;
  bme->error_count = system->bme.error_count;
  bme->recovery_request_count = system->bme.recovery_request_count;
  bme->recovery_success_count = system->bme.recovery_success_count;

  app_veml7700_snapshot_t *veml = &system->inputs.veml7700;
  veml->state = system->veml.state;
  veml->status = system->veml.status;
  veml->last_error_status = system->veml.last_error_status;
  veml->last_transport_result = system->veml.last_transport_result;
  veml->sample = system->veml.sample;
  veml->transaction_count = system->veml.transaction_count;
  veml->error_count = system->veml.error_count;
  veml->range_change_count = system->veml.range_change_count;
  veml->recovery_request_count = system->veml.recovery_request_count;
  veml->recovery_success_count = system->veml.recovery_success_count;

  app_adxl345_snapshot_t *adxl = &system->inputs.adxl345;
  adxl->state = system->adxl.state;
  adxl->status = system->adxl.status;
  adxl->last_error_status = system->adxl.last_error_status;
  adxl->last_transport_result = system->adxl.last_transport_result;
  adxl->sample = system->adxl.sample;
  adxl->feature = system->adxl.feature;
  adxl->feature_window_count = system->adxl.window.count;
  adxl->transaction_count = system->adxl.transaction_count;
  adxl->error_count = system->adxl.error_count;
  adxl->recovery_request_count = system->adxl.recovery_request_count;
  adxl->recovery_success_count = system->adxl.recovery_success_count;
  adxl->irq_event_count = system->adxl.irq_event_count;
  adxl->dropped_sample_lower_bound =
      system->adxl.dropped_sample_lower_bound;
}

static bool combined_publish(combined_system_t *system, uint32_t now_ms)
{
  combined_project_driver_snapshots(system);
  return app_measurement_update(
             &system->measurement_model, &system->inputs, now_ms) &&
         app_measurement_get_snapshot(
             &system->measurement_model, now_ms, &system->measurement) &&
         app_sensor_monitor_update(&system->monitor,
                                   &system->inputs,
                                   &system->measurement,
                                   now_ms) &&
         app_sensor_monitor_get_snapshot(
             &system->monitor, &system->monitor_snapshot);
}

static bool combined_step(combined_system_t *system, uint32_t now_ms)
{
  if ((now_ms % UINT32_C(20)) == 0U)
  {
    uint32_t before = system->adxl_bus.transaction_count;
    if (!adxl345_service(&system->adxl, now_ms, 0U) ||
        ((system->adxl_bus.transaction_count - before) > 1U))
    {
      return false;
    }
    before = system->bme_bus.transaction_count;
    if (!bme280_service(&system->bme, now_ms) ||
        ((system->bme_bus.transaction_count - before) > 1U))
    {
      return false;
    }
    before = system->veml_bus.transaction_count;
    if (!veml7700_service(&system->veml, now_ms) ||
        ((system->veml_bus.transaction_count - before) > 1U))
    {
      return false;
    }
  }

  if (((now_ms % UINT32_C(10)) == 0U) &&
      (system->adxl.state == ADXL345_STATE_WAIT_DATA_READY))
  {
    const uint32_t before = system->adxl_bus.transaction_count;
    if (!adxl345_service(&system->adxl, now_ms, 1U) ||
        ((system->adxl_bus.transaction_count - before) > 1U))
    {
      return false;
    }
  }
  return combined_publish(system, now_ms);
}

static bool combined_run(combined_system_t *system,
                         uint32_t start_ms,
                         uint32_t finish_ms)
{
  for (uint32_t now = start_ms; now <= finish_ms; now += UINT32_C(10))
  {
    if (!combined_step(system, now))
    {
      return false;
    }
  }
  return true;
}

static void health_advance_tasks(app_health_input_t *input)
{
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    if (index != (size_t)APP_TASK_HEALTH)
    {
      ++input->task_release_count[index];
    }
  }
}

static int test_monitor_wrap_and_recovery_duration(void)
{
  app_sensor_monitor_t monitor;
  app_sensor_monitor_snapshot_t snapshot;
  app_measurement_inputs_t drivers = {0};
  app_measurement_snapshot_t measurement = {0};
  measurement.schema_revision = APP_MEASUREMENT_SCHEMA_REVISION;
  measurement.bme280.metadata.source = APP_MEASUREMENT_SOURCE_BME280;
  measurement.bme280.metadata.value_present = true;
  measurement.bme280.metadata.state = APP_MEASUREMENT_STATE_FRESH;
  measurement.bme280.metadata.sequence = 1U;
  measurement.bme280.metadata.sample_monotonic_ms = UINT32_MAX - 5U;
  drivers.bme280.last_transport_result = BME280_TRANSPORT_OK;
  drivers.veml7700.last_transport_result = VEML7700_TRANSPORT_OK;
  drivers.adxl345.last_transport_result = ADXL345_TRANSPORT_OK;
  app_sensor_monitor_initialize(&monitor);
  CHECK(app_sensor_monitor_update(
      &monitor, &drivers, &measurement, UINT32_MAX - 5U));

  measurement.bme280.metadata.sequence = 2U;
  measurement.bme280.metadata.sample_monotonic_ms = 3U;
  drivers.bme280.status = BME280_STATUS_RECOVERY_REQUIRED;
  drivers.bme280.recovery_request_count = 1U;
  CHECK(app_sensor_monitor_update(&monitor, &drivers, &measurement, 3U));
  CHECK(app_sensor_monitor_get_snapshot(&monitor, &snapshot));
  CHECK(snapshot.sample[APP_MEASUREMENT_SOURCE_BME280].last_interval_ms ==
        9U);
  CHECK(snapshot.device[APP_SENSOR_DEVICE_BME280].recovery_active);

  drivers.bme280.status = BME280_STATUS_VALID;
  drivers.bme280.recovery_success_count = 1U;
  CHECK(app_sensor_monitor_update(&monitor, &drivers, &measurement, 8U));
  CHECK(app_sensor_monitor_get_snapshot(&monitor, &snapshot));
  CHECK(snapshot.device[APP_SENSOR_DEVICE_BME280]
            .last_recovery_duration_ms == 5U);
  CHECK(snapshot.device[APP_SENSOR_DEVICE_BME280]
            .recovery_duration_valid);
  CHECK(!snapshot.device[APP_SENSOR_DEVICE_BME280].recovery_active);
  CHECK(!app_sensor_monitor_update(NULL, &drivers, &measurement, 0U));
  CHECK(!app_sensor_monitor_get_snapshot(&monitor, NULL));
  return EXIT_SUCCESS;
}

static int test_healthy_and_bme_timeout_isolation(void)
{
  combined_system_t system;
  CHECK(combined_initialize(&system));
  CHECK(combined_run(&system, 0U, 3500U));
  CHECK(system.bme.sample.sequence >= 2U);
  CHECK(system.veml.sample.sequence >= 2U);
  CHECK(system.adxl.sample.sequence >= 100U);
  CHECK(system.adxl.feature.sequence >= 1U);
  CHECK(system.monitor_snapshot.unavailable_device_mask == 0U);

  const uint32_t veml_before = system.veml.sample.sequence;
  const uint32_t adxl_before = system.adxl.sample.sequence;
  system.bme_bus.force_result = true;
  system.bme_bus.forced_result = BME280_TRANSPORT_TIMEOUT;
  CHECK(combined_run(&system, 3510U, 6500U));
  CHECK(system.bme.state == BME280_STATE_OFFLINE);
  CHECK(system.veml.sample.sequence > veml_before);
  CHECK(system.adxl.sample.sequence > adxl_before);
  CHECK((system.monitor_snapshot.unavailable_device_mask &
         (UINT32_C(1) << APP_SENSOR_DEVICE_BME280)) != 0U);
  CHECK(system.measurement.bme280.metadata.value_present);
  CHECK(system.measurement.bme280.metadata.value_is_retained);
  CHECK(system.monitor_snapshot.device[APP_SENSOR_DEVICE_BME280]
            .fault_episode_count == 1U);

  app_health_policy_t health;
  app_health_input_t input = {0};
  app_health_decision_t decision;
  input.queue_depth = APP_TRANSPORT_EVENT_QUEUE_DEPTH;
  input.sensor_unavailable_mask =
      system.monitor_snapshot.unavailable_device_mask;
  input.sensor_stale_mask = system.monitor_snapshot.stale_source_mask;
  input.sensor_recovery_mask = system.monitor_snapshot.recovery_device_mask;
  app_health_policy_initialize(&health);
  CHECK(app_health_policy_evaluate(&health, &input, &decision));
  health_advance_tasks(&input);
  CHECK(app_health_policy_evaluate(&health, &input, &decision));
  CHECK(decision.state == APP_HEALTH_DEGRADED);
  CHECK(decision.feed_decision == APP_WATCHDOG_FEED_ALLOWED);
  CHECK(decision.recovery_attempts == 0U);
  return EXIT_SUCCESS;
}

static int test_identity_and_veml_fault_isolation(void)
{
  combined_system_t identity;
  CHECK(combined_initialize(&identity));
  identity.bme_bus.chip_id = BME280_BMP280_CHIP_ID;
  CHECK(combined_run(&identity, 0U, 2500U));
  CHECK(identity.bme.state == BME280_STATE_OFFLINE);
  CHECK(identity.bme.sample.sequence == 0U);
  CHECK(identity.veml.sample.sequence != 0U);
  CHECK(identity.adxl.sample.sequence != 0U);
  CHECK(identity.monitor_snapshot.device[APP_SENSOR_DEVICE_BME280]
            .fault_class == APP_SENSOR_FAULT_NOT_PRESENT_OR_IDENTITY);

  combined_system_t veml_fault;
  CHECK(combined_initialize(&veml_fault));
  CHECK(combined_run(&veml_fault, 0U, 3500U));
  const uint32_t bme_before = veml_fault.bme.sample.sequence;
  const uint32_t adxl_before = veml_fault.adxl.sample.sequence;
  veml_fault.veml_bus.force_result = true;
  veml_fault.veml_bus.forced_result = VEML7700_TRANSPORT_NOT_PRESENT;
  CHECK(combined_run(&veml_fault, 3510U, 5500U));
  CHECK(veml_fault.veml.state == VEML7700_STATE_OFFLINE);
  CHECK(veml_fault.bme.sample.sequence > bme_before);
  CHECK(veml_fault.adxl.sample.sequence > adxl_before);
  CHECK(veml_fault.monitor_snapshot.device[APP_SENSOR_DEVICE_VEML7700]
            .fault_class == APP_SENSOR_FAULT_NOT_PRESENT_OR_IDENTITY);

  combined_system_t config_fault;
  CHECK(combined_initialize(&config_fault));
  config_fault.veml_bus.ignore_config_write = true;
  CHECK(combined_run(&config_fault, 0U, 2500U));
  CHECK(config_fault.veml.state == VEML7700_STATE_OFFLINE);
  CHECK(config_fault.monitor_snapshot.device[APP_SENSOR_DEVICE_VEML7700]
            .fault_class == APP_SENSOR_FAULT_CONFIGURATION);
  CHECK(config_fault.bme.sample.sequence != 0U);
  CHECK(config_fault.adxl.sample.sequence != 0U);
  return EXIT_SUCCESS;
}

static int test_adxl_repeated_irq_timeout_and_queue_bound(void)
{
  combined_system_t system;
  CHECK(combined_initialize(&system));
  CHECK(combined_run(&system, 0U, 3500U));
  CHECK(system.adxl.state == ADXL345_STATE_WAIT_DATA_READY);
  const uint32_t reads_before = system.adxl_bus.data_read_count;
  const uint32_t dropped_before = system.adxl.dropped_sample_lower_bound;
  CHECK(adxl345_service(&system.adxl, 3501U, 3U));
  CHECK(system.adxl_bus.data_read_count == reads_before + 1U);
  CHECK(system.adxl.dropped_sample_lower_bound == dropped_before + 2U);
  CHECK(combined_publish(&system, 3501U));

  const uint32_t bme_before = system.bme.sample.sequence;
  const uint32_t veml_before = system.veml.sample.sequence;
  system.adxl_bus.force_result = true;
  system.adxl_bus.forced_result = ADXL345_TRANSPORT_TIMEOUT;
  CHECK(combined_run(&system, 3510U, 5500U));
  CHECK(system.adxl.state == ADXL345_STATE_OFFLINE);
  CHECK(system.bme.sample.sequence > bme_before);
  CHECK(system.veml.sample.sequence > veml_before);
  CHECK((system.monitor_snapshot.unavailable_device_mask &
         (UINT32_C(1) << APP_SENSOR_DEVICE_ADXL345)) != 0U);
  CHECK(system.monitor_snapshot.adxl345_dropped_sample_lower_bound >=
        dropped_before + 3U);
  CHECK(app_transport_event_drain_count(
            APP_TRANSPORT_EVENT_QUEUE_DEPTH) ==
        APP_TRANSPORT_EVENT_DRAIN_BUDGET);
  CHECK(app_transport_event_drain_count(1U) == 1U);
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(test_monitor_wrap_and_recovery_duration() == EXIT_SUCCESS);
  CHECK(test_healthy_and_bme_timeout_isolation() == EXIT_SUCCESS);
  CHECK(test_identity_and_veml_fault_isolation() == EXIT_SUCCESS);
  CHECK(test_adxl_repeated_irq_timeout_and_queue_bound() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
