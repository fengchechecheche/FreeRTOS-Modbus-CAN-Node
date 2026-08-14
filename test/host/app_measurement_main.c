#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_measurement.h"

#define CHECK(condition)                                                   \
  do                                                                       \
  {                                                                        \
    if (!(condition))                                                      \
    {                                                                      \
      (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,     \
                    __LINE__, #condition);                                 \
      return EXIT_FAILURE;                                                 \
    }                                                                      \
  } while (0)

static void make_bme_valid(app_measurement_inputs_t *inputs,
                           uint32_t sequence)
{
  inputs->bme280.state = BME280_STATE_IDLE;
  inputs->bme280.status = BME280_STATUS_VALID;
  inputs->bme280.last_transport_result = BME280_TRANSPORT_OK;
  inputs->bme280.sample.status = BME280_STATUS_VALID;
  inputs->bme280.sample.valid_mask = BME280_SAMPLE_VALID_ALL;
  inputs->bme280.sample.sequence = sequence;
  inputs->bme280.sample.temperature_centi_c = -1234;
  inputs->bme280.sample.pressure_pa = 100123U;
  inputs->bme280.sample.humidity_milli_pct = 45678U;
}

static void make_veml_valid(app_measurement_inputs_t *inputs,
                            uint32_t sequence)
{
  inputs->veml7700.state = VEML7700_STATE_IDLE;
  inputs->veml7700.status = VEML7700_STATUS_VALID;
  inputs->veml7700.last_transport_result = VEML7700_TRANSPORT_OK;
  inputs->veml7700.sample.status = VEML7700_STATUS_VALID;
  inputs->veml7700.sample.quality_flags = VEML7700_QUALITY_VALID;
  inputs->veml7700.sample.sequence = sequence;
  inputs->veml7700.sample.illuminance_millilux = 7654321U;
}

static void make_adxl_valid(app_measurement_inputs_t *inputs,
                            uint32_t sample_sequence,
                            uint32_t feature_sequence)
{
  inputs->adxl345.state = ADXL345_STATE_WAIT_DATA_READY;
  inputs->adxl345.status = ADXL345_STATUS_VALID;
  inputs->adxl345.last_transport_result = ADXL345_TRANSPORT_OK;
  inputs->adxl345.sample.status = ADXL345_STATUS_VALID;
  inputs->adxl345.sample.sequence = sample_sequence;
  inputs->adxl345.sample.quality_flags =
      ADXL345_QUALITY_VALID | ADXL345_QUALITY_BIAS_UNCALIBRATED;
  inputs->adxl345.sample.acceleration_millig[0] = -1000;
  inputs->adxl345.sample.acceleration_millig[1] = 0;
  inputs->adxl345.sample.acceleration_millig[2] = 1000;
  inputs->adxl345.feature.sequence = feature_sequence;
  inputs->adxl345.feature.sample_count = ADXL345_FEATURE_WINDOW_SAMPLES;
  inputs->adxl345.feature.quality_flags = ADXL345_QUALITY_VALID;
  inputs->adxl345.feature.mean_millig[0] = -10;
  inputs->adxl345.feature.mean_millig[1] = 20;
  inputs->adxl345.feature.mean_millig[2] = 30;
  inputs->adxl345.feature.rms_millig[0] = 100U;
  inputs->adxl345.feature.rms_millig[1] = 200U;
  inputs->adxl345.feature.rms_millig[2] = 300U;
  inputs->adxl345.feature.peak_abs_millig[0] = 400U;
  inputs->adxl345.feature.peak_abs_millig[1] = 500U;
  inputs->adxl345.feature.peak_abs_millig[2] = 600U;
  inputs->adxl345.feature.resultant_rms_millig = 374U;
}

static int test_initial_invalid_and_checked_accessor(void)
{
  app_measurement_model_t model;
  app_measurement_snapshot_t snapshot;
  app_measurement_model_initialize(&model);
  CHECK(app_measurement_get_snapshot(&model, 123U, &snapshot));
  CHECK(snapshot.schema_revision == APP_MEASUREMENT_SCHEMA_REVISION);
  CHECK(snapshot.bme280.metadata.state == APP_MEASUREMENT_STATE_INVALID);
  CHECK(snapshot.veml7700.metadata.state == APP_MEASUREMENT_STATE_INVALID);
  CHECK(snapshot.adxl345_sample.metadata.state ==
        APP_MEASUREMENT_STATE_INVALID);
  CHECK(snapshot.adxl345_feature.metadata.state ==
        APP_MEASUREMENT_STATE_INVALID);

  int64_t value = INT64_C(0x12345678);
  app_measurement_metadata_t metadata;
  CHECK(!app_measurement_read_field(
      &snapshot,
      APP_MEASUREMENT_FIELD_BME280_TEMPERATURE,
      &value,
      &metadata));
  CHECK(value == INT64_C(0x12345678));
  CHECK(metadata.state == APP_MEASUREMENT_STATE_INVALID);
  CHECK(!metadata.value_present);
  return EXIT_SUCCESS;
}

static int test_first_good_thresholds_and_sequence(void)
{
  app_measurement_model_t model;
  app_measurement_inputs_t inputs;
  app_measurement_snapshot_t snapshot;
  (void)memset(&inputs, 0, sizeof(inputs));
  app_measurement_model_initialize(&model);
  make_bme_valid(&inputs, 1U);
  CHECK(app_measurement_update(&model, &inputs, 100U));
  CHECK(app_measurement_get_snapshot(&model, 100U, &snapshot));
  CHECK(snapshot.bme280.metadata.state == APP_MEASUREMENT_STATE_FRESH);
  CHECK(snapshot.bme280.metadata.sequence == 1U);
  CHECK(snapshot.bme280.metadata.sample_monotonic_ms == 100U);
  CHECK(snapshot.bme280.metadata.age_ms == 0U);
  CHECK(snapshot.bme280.temperature_centi_c == -1234);

  CHECK(app_measurement_update(&model, &inputs, 200U));
  CHECK(app_measurement_get_snapshot(&model, 2599U, &snapshot));
  CHECK(snapshot.bme280.metadata.state == APP_MEASUREMENT_STATE_FRESH);
  CHECK(app_measurement_get_snapshot(&model, 2600U, &snapshot));
  CHECK(snapshot.bme280.metadata.sample_monotonic_ms == 100U);
  CHECK(snapshot.bme280.metadata.age_ms ==
        APP_MEASUREMENT_BME280_FRESH_MS);
  CHECK(snapshot.bme280.metadata.state == APP_MEASUREMENT_STATE_FRESH);
  CHECK(!snapshot.bme280.metadata.value_is_retained);
  CHECK(app_measurement_get_snapshot(&model, 2601U, &snapshot));
  CHECK(snapshot.bme280.metadata.state == APP_MEASUREMENT_STATE_STALE);
  CHECK(snapshot.bme280.metadata.value_is_retained);

  make_bme_valid(&inputs, 2U);
  CHECK(app_measurement_update(&model, &inputs, 3000U));
  CHECK(app_measurement_get_snapshot(&model, 3000U, &snapshot));
  CHECK(snapshot.bme280.metadata.sequence == 2U);
  CHECK(snapshot.bme280.metadata.sample_monotonic_ms == 3000U);
  CHECK(snapshot.bme280.metadata.state == APP_MEASUREMENT_STATE_FRESH);
  return EXIT_SUCCESS;
}

static int test_tick_and_sequence_wrap(void)
{
  app_measurement_model_t model;
  app_measurement_inputs_t inputs;
  app_measurement_snapshot_t snapshot;
  (void)memset(&inputs, 0, sizeof(inputs));
  app_measurement_model_initialize(&model);
  make_bme_valid(&inputs, UINT32_MAX);
  CHECK(app_measurement_update(&model, &inputs, UINT32_MAX - 10U));
  CHECK(app_measurement_get_snapshot(&model, 5U, &snapshot));
  CHECK(snapshot.bme280.metadata.age_ms == 16U);
  CHECK(snapshot.bme280.metadata.state == APP_MEASUREMENT_STATE_FRESH);

  make_bme_valid(&inputs, 0U);
  inputs.bme280.sample.temperature_centi_c = 2500;
  CHECK(app_measurement_update(&model, &inputs, 6U));
  CHECK(app_measurement_get_snapshot(&model, 6U, &snapshot));
  CHECK(snapshot.bme280.metadata.sequence == 0U);
  CHECK(snapshot.bme280.metadata.sample_monotonic_ms == 6U);
  CHECK(snapshot.bme280.temperature_centi_c == 2500);
  return EXIT_SUCCESS;
}

static int test_offline_retained_and_invalid_mask(void)
{
  app_measurement_model_t model;
  app_measurement_inputs_t inputs;
  app_measurement_snapshot_t snapshot;
  (void)memset(&inputs, 0, sizeof(inputs));
  app_measurement_model_initialize(&model);

  inputs.bme280.state = BME280_STATE_OFFLINE;
  inputs.bme280.status = BME280_STATUS_OFFLINE;
  CHECK(app_measurement_update(&model, &inputs, 10U));
  CHECK(app_measurement_get_snapshot(&model, 20U, &snapshot));
  CHECK(snapshot.bme280.metadata.state == APP_MEASUREMENT_STATE_OFFLINE);
  CHECK(!snapshot.bme280.metadata.value_present);
  CHECK(!snapshot.bme280.metadata.value_is_retained);

  make_bme_valid(&inputs, 1U);
  inputs.bme280.sample.valid_mask = BME280_SAMPLE_VALID_TEMPERATURE;
  CHECK(app_measurement_update(&model, &inputs, 30U));
  CHECK(app_measurement_get_snapshot(&model, 30U, &snapshot));
  CHECK(!snapshot.bme280.metadata.value_present);

  inputs.bme280.sample.valid_mask = BME280_SAMPLE_VALID_ALL;
  CHECK(app_measurement_update(&model, &inputs, 40U));
  inputs.bme280.state = BME280_STATE_OFFLINE;
  inputs.bme280.status = BME280_STATUS_OFFLINE;
  inputs.bme280.last_transport_result = BME280_TRANSPORT_TIMEOUT;
  CHECK(app_measurement_update(&model, &inputs, 50U));
  CHECK(app_measurement_get_snapshot(&model, 60U, &snapshot));
  CHECK(snapshot.bme280.metadata.state == APP_MEASUREMENT_STATE_OFFLINE);
  CHECK(snapshot.bme280.metadata.value_present);
  CHECK(snapshot.bme280.metadata.value_is_retained);
  CHECK((snapshot.bme280.metadata.quality_flags &
         APP_MEASUREMENT_QUALITY_TRANSPORT_ERROR) != 0U);
  return EXIT_SUCCESS;
}

static int test_sources_quality_and_independent_feature_time(void)
{
  app_measurement_model_t model;
  app_measurement_inputs_t inputs;
  app_measurement_snapshot_t snapshot;
  (void)memset(&inputs, 0, sizeof(inputs));
  app_measurement_model_initialize(&model);
  make_veml_valid(&inputs, 1U);
  make_adxl_valid(&inputs, 10U, 1U);
  inputs.veml7700.sample.quality_flags |=
      VEML7700_QUALITY_SATURATED | VEML7700_QUALITY_RANGE_LIMITED;
  inputs.adxl345.sample.quality_flags |=
      ADXL345_QUALITY_GAP | ADXL345_QUALITY_DROPPED;
  CHECK(app_measurement_update(&model, &inputs, 1000U));
  CHECK(app_measurement_get_snapshot(&model, 1000U, &snapshot));
  CHECK(snapshot.veml7700.metadata.state == APP_MEASUREMENT_STATE_FRESH);
  CHECK((snapshot.veml7700.metadata.quality_flags &
         APP_MEASUREMENT_QUALITY_RANGE_WARNING) != 0U);
  CHECK((snapshot.veml7700.metadata.quality_flags &
         APP_MEASUREMENT_QUALITY_SATURATED) != 0U);
  CHECK((snapshot.adxl345_sample.metadata.quality_flags &
         APP_MEASUREMENT_QUALITY_UNCALIBRATED) != 0U);
  CHECK((snapshot.adxl345_sample.metadata.quality_flags &
         APP_MEASUREMENT_QUALITY_GAP) != 0U);
  CHECK((snapshot.adxl345_sample.metadata.quality_flags &
         APP_MEASUREMENT_QUALITY_DROPPED) != 0U);

  CHECK(app_measurement_get_snapshot(
      &model,
      1000U + APP_MEASUREMENT_ADXL345_SAMPLE_FRESH_MS,
      &snapshot));
  CHECK(snapshot.adxl345_sample.metadata.state ==
        APP_MEASUREMENT_STATE_FRESH);
  CHECK(app_measurement_get_snapshot(
      &model,
      1001U + APP_MEASUREMENT_ADXL345_SAMPLE_FRESH_MS,
      &snapshot));
  CHECK(snapshot.adxl345_sample.metadata.state ==
        APP_MEASUREMENT_STATE_STALE);
  CHECK(app_measurement_get_snapshot(
      &model,
      1000U + APP_MEASUREMENT_VEML7700_FRESH_MS,
      &snapshot));
  CHECK(snapshot.veml7700.metadata.state == APP_MEASUREMENT_STATE_FRESH);
  CHECK(app_measurement_get_snapshot(
      &model,
      1001U + APP_MEASUREMENT_VEML7700_FRESH_MS,
      &snapshot));
  CHECK(snapshot.veml7700.metadata.state == APP_MEASUREMENT_STATE_STALE);
  CHECK(app_measurement_get_snapshot(
      &model,
      1000U + APP_MEASUREMENT_ADXL345_FEATURE_FRESH_MS,
      &snapshot));
  CHECK(snapshot.adxl345_feature.metadata.state ==
        APP_MEASUREMENT_STATE_FRESH);
  CHECK(app_measurement_get_snapshot(
      &model,
      1001U + APP_MEASUREMENT_ADXL345_FEATURE_FRESH_MS,
      &snapshot));
  CHECK(snapshot.adxl345_feature.metadata.state ==
        APP_MEASUREMENT_STATE_STALE);

  make_adxl_valid(&inputs, 11U, 1U);
  CHECK(app_measurement_update(&model, &inputs, 1010U));
  CHECK(app_measurement_get_snapshot(&model, 1010U, &snapshot));
  CHECK(snapshot.adxl345_sample.metadata.sample_monotonic_ms == 1010U);
  CHECK(snapshot.adxl345_feature.metadata.sample_monotonic_ms == 1000U);
  CHECK(snapshot.adxl345_feature.metadata.sequence == 1U);

  inputs.adxl345.feature.quality_flags = 0U;
  inputs.adxl345.feature.sequence = 2U;
  CHECK(app_measurement_update(&model, &inputs, 1020U));
  CHECK(app_measurement_get_snapshot(&model, 1020U, &snapshot));
  CHECK(snapshot.adxl345_feature.metadata.sequence == 1U);
  CHECK(snapshot.adxl345_feature.metadata.sample_monotonic_ms == 1000U);
  return EXIT_SUCCESS;
}

static int test_dictionary_and_field_values(void)
{
  app_measurement_field_descriptor_t descriptors[17];
  CHECK(app_measurement_field_count() == 17U);
  for (size_t index = 0U; index < app_measurement_field_count(); ++index)
  {
    CHECK(app_measurement_field_descriptor(index, &descriptors[index]));
    for (size_t previous = 0U; previous < index; ++previous)
    {
      CHECK(descriptors[index].field_id != descriptors[previous].field_id);
    }
  }
  CHECK(descriptors[0].source == APP_MEASUREMENT_SOURCE_BME280);
  CHECK(descriptors[0].unit == APP_MEASUREMENT_UNIT_CENTI_DEG_C);
  CHECK(descriptors[1].source == APP_MEASUREMENT_SOURCE_BME280);
  CHECK(descriptors[1].unit == APP_MEASUREMENT_UNIT_PA);
  CHECK(descriptors[2].source == APP_MEASUREMENT_SOURCE_BME280);
  CHECK(descriptors[2].unit == APP_MEASUREMENT_UNIT_MILLI_PERCENT_RH);
  CHECK(descriptors[3].source == APP_MEASUREMENT_SOURCE_VEML7700);
  CHECK(descriptors[3].unit == APP_MEASUREMENT_UNIT_MILLILUX);
  for (size_t index = 4U; index < 7U; ++index)
  {
    CHECK(descriptors[index].source ==
          APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE);
    CHECK(descriptors[index].unit == APP_MEASUREMENT_UNIT_MILLIG);
  }
  for (size_t index = 7U; index < 17U; ++index)
  {
    CHECK(descriptors[index].source ==
          APP_MEASUREMENT_SOURCE_ADXL345_FEATURE);
    CHECK(descriptors[index].unit == APP_MEASUREMENT_UNIT_MILLIG);
  }
  CHECK(!app_measurement_field_descriptor(17U, &descriptors[0]));

  app_measurement_model_t model;
  app_measurement_inputs_t inputs;
  app_measurement_snapshot_t snapshot;
  (void)memset(&inputs, 0, sizeof(inputs));
  app_measurement_model_initialize(&model);
  make_bme_valid(&inputs, 1U);
  make_veml_valid(&inputs, 1U);
  make_adxl_valid(&inputs, 1U, 1U);
  CHECK(app_measurement_update(&model, &inputs, 500U));
  CHECK(app_measurement_get_snapshot(&model, 500U, &snapshot));

  int64_t value = 0;
  CHECK(app_measurement_read_field(
      &snapshot, APP_MEASUREMENT_FIELD_BME280_TEMPERATURE, &value, NULL));
  CHECK(value == -1234);
  CHECK(app_measurement_read_field(
      &snapshot, APP_MEASUREMENT_FIELD_VEML7700_ILLUMINANCE, &value, NULL));
  CHECK(value == 7654321);
  CHECK(app_measurement_read_field(
      &snapshot, APP_MEASUREMENT_FIELD_ADXL345_ACCELERATION_X, &value, NULL));
  CHECK(value == -1000);
  CHECK(app_measurement_read_field(
      &snapshot, APP_MEASUREMENT_FIELD_ADXL345_RMS_Z, &value, NULL));
  CHECK(value == 300);
  CHECK(app_measurement_read_field(
      &snapshot, APP_MEASUREMENT_FIELD_ADXL345_RESULTANT_RMS, &value, NULL));
  CHECK(value == 374);
  value = INT64_C(0x55aa);
  CHECK(!app_measurement_read_field(
      &snapshot, (app_measurement_field_id_t)0xffff, &value, NULL));
  CHECK(value == INT64_C(0x55aa));
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(test_initial_invalid_and_checked_accessor() == EXIT_SUCCESS);
  CHECK(test_first_good_thresholds_and_sequence() == EXIT_SUCCESS);
  CHECK(test_tick_and_sequence_wrap() == EXIT_SUCCESS);
  CHECK(test_offline_retained_and_invalid_mask() == EXIT_SUCCESS);
  CHECK(test_sources_quality_and_independent_feature_time() == EXIT_SUCCESS);
  CHECK(test_dictionary_and_field_values() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
