#include "app_measurement.h"

#include <string.h>

#define APP_MEASUREMENT_FIELD_COUNT (17U)

static const app_measurement_field_descriptor_t app_measurement_fields[] = {
    {APP_MEASUREMENT_FIELD_BME280_TEMPERATURE,
     APP_MEASUREMENT_SOURCE_BME280,
     APP_MEASUREMENT_UNIT_CENTI_DEG_C},
    {APP_MEASUREMENT_FIELD_BME280_PRESSURE,
     APP_MEASUREMENT_SOURCE_BME280,
     APP_MEASUREMENT_UNIT_PA},
    {APP_MEASUREMENT_FIELD_BME280_HUMIDITY,
     APP_MEASUREMENT_SOURCE_BME280,
     APP_MEASUREMENT_UNIT_MILLI_PERCENT_RH},
    {APP_MEASUREMENT_FIELD_VEML7700_ILLUMINANCE,
     APP_MEASUREMENT_SOURCE_VEML7700,
     APP_MEASUREMENT_UNIT_MILLILUX},
    {APP_MEASUREMENT_FIELD_ADXL345_ACCELERATION_X,
     APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_ACCELERATION_Y,
     APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_ACCELERATION_Z,
     APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_MEAN_X,
     APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_MEAN_Y,
     APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_MEAN_Z,
     APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_RMS_X,
     APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_RMS_Y,
     APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_RMS_Z,
     APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_PEAK_X,
     APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_PEAK_Y,
     APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_PEAK_Z,
     APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
     APP_MEASUREMENT_UNIT_MILLIG},
    {APP_MEASUREMENT_FIELD_ADXL345_RESULTANT_RMS,
     APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
     APP_MEASUREMENT_UNIT_MILLIG},
};

_Static_assert(
    (sizeof(app_measurement_fields) / sizeof(app_measurement_fields[0])) ==
        APP_MEASUREMENT_FIELD_COUNT,
    "measurement field dictionary count changed");

static app_measurement_metadata_t *app_measurement_metadata(
    app_measurement_snapshot_t *snapshot,
    app_measurement_source_t source)
{
  switch (source)
  {
    case APP_MEASUREMENT_SOURCE_BME280:
      return &snapshot->bme280.metadata;
    case APP_MEASUREMENT_SOURCE_VEML7700:
      return &snapshot->veml7700.metadata;
    case APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE:
      return &snapshot->adxl345_sample.metadata;
    case APP_MEASUREMENT_SOURCE_ADXL345_FEATURE:
      return &snapshot->adxl345_feature.metadata;
    case APP_MEASUREMENT_SOURCE_COUNT:
    default:
      return NULL;
  }
}

static const app_measurement_metadata_t *app_measurement_const_metadata(
    const app_measurement_snapshot_t *snapshot,
    app_measurement_source_t source)
{
  switch (source)
  {
    case APP_MEASUREMENT_SOURCE_BME280:
      return &snapshot->bme280.metadata;
    case APP_MEASUREMENT_SOURCE_VEML7700:
      return &snapshot->veml7700.metadata;
    case APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE:
      return &snapshot->adxl345_sample.metadata;
    case APP_MEASUREMENT_SOURCE_ADXL345_FEATURE:
      return &snapshot->adxl345_feature.metadata;
    case APP_MEASUREMENT_SOURCE_COUNT:
    default:
      return NULL;
  }
}

static uint32_t app_measurement_fresh_limit(app_measurement_source_t source)
{
  switch (source)
  {
    case APP_MEASUREMENT_SOURCE_BME280:
      return APP_MEASUREMENT_BME280_FRESH_MS;
    case APP_MEASUREMENT_SOURCE_VEML7700:
      return APP_MEASUREMENT_VEML7700_FRESH_MS;
    case APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE:
      return APP_MEASUREMENT_ADXL345_SAMPLE_FRESH_MS;
    case APP_MEASUREMENT_SOURCE_ADXL345_FEATURE:
      return APP_MEASUREMENT_ADXL345_FEATURE_FRESH_MS;
    case APP_MEASUREMENT_SOURCE_COUNT:
    default:
      return 0U;
  }
}

static bool app_measurement_transport_failed_bme280(
    bme280_transport_result_t result)
{
  return result != BME280_TRANSPORT_OK;
}

static bool app_measurement_transport_failed_veml7700(
    veml7700_transport_result_t result)
{
  return result != VEML7700_TRANSPORT_OK;
}

static bool app_measurement_transport_failed_adxl345(
    adxl345_transport_result_t result)
{
  return result != ADXL345_TRANSPORT_OK;
}

static uint32_t app_measurement_bme280_quality(
    const app_bme280_snapshot_t *snapshot)
{
  uint32_t quality = 0U;
  if (app_measurement_transport_failed_bme280(
          snapshot->last_transport_result))
  {
    quality |= APP_MEASUREMENT_QUALITY_TRANSPORT_ERROR;
  }
  if ((snapshot->status == BME280_STATUS_NVM_TIMEOUT) ||
      (snapshot->status == BME280_STATUS_MEASUREMENT_TIMEOUT))
  {
    quality |= APP_MEASUREMENT_QUALITY_TRANSPORT_ERROR;
  }
  if ((snapshot->status == BME280_STATUS_WRONG_ID) ||
      (snapshot->status == BME280_STATUS_UNSUPPORTED_BMP280) ||
      (snapshot->status == BME280_STATUS_CALIBRATION_INVALID) ||
      (snapshot->status == BME280_STATUS_MEASUREMENT_INVALID))
  {
    quality |= APP_MEASUREMENT_QUALITY_CONFIGURATION_ERROR;
  }
  if (snapshot->status == BME280_STATUS_RECOVERY_REQUIRED)
  {
    quality |= APP_MEASUREMENT_QUALITY_RECOVERY_ACTIVE;
  }
  return quality;
}

static uint32_t app_measurement_veml7700_quality(
    const app_veml7700_snapshot_t *snapshot)
{
  uint32_t quality = 0U;
  const uint32_t source_quality = snapshot->sample.quality_flags;
  if ((source_quality & (VEML7700_QUALITY_RANGING |
                         VEML7700_QUALITY_LOW_COUNT |
                         VEML7700_QUALITY_RANGE_LIMITED |
                         VEML7700_QUALITY_HIGH_LUX_UNCORRECTED)) != 0U)
  {
    quality |= APP_MEASUREMENT_QUALITY_RANGE_WARNING;
  }
  if (snapshot->status == VEML7700_STATUS_RANGING)
  {
    quality |= APP_MEASUREMENT_QUALITY_RANGE_WARNING;
  }
  if ((source_quality & VEML7700_QUALITY_SATURATED) != 0U)
  {
    quality |= APP_MEASUREMENT_QUALITY_SATURATED;
  }
  if (app_measurement_transport_failed_veml7700(
          snapshot->last_transport_result))
  {
    quality |= APP_MEASUREMENT_QUALITY_TRANSPORT_ERROR;
  }
  if (snapshot->status == VEML7700_STATUS_CONFIGURATION_MISMATCH)
  {
    quality |= APP_MEASUREMENT_QUALITY_CONFIGURATION_ERROR;
  }
  if ((snapshot->status == VEML7700_STATUS_RECOVERY_REQUIRED) ||
      (snapshot->status == VEML7700_STATUS_RECOVERY_UNAVAILABLE))
  {
    quality |= APP_MEASUREMENT_QUALITY_RECOVERY_ACTIVE;
  }
  return quality;
}

static uint32_t app_measurement_adxl345_quality(
    const app_adxl345_snapshot_t *snapshot,
    uint32_t source_quality)
{
  uint32_t quality = 0U;
  if ((source_quality & ADXL345_QUALITY_BIAS_UNCALIBRATED) != 0U)
  {
    quality |= APP_MEASUREMENT_QUALITY_UNCALIBRATED;
  }
  if ((source_quality & (ADXL345_QUALITY_GAP |
                         ADXL345_QUALITY_STALLED)) != 0U)
  {
    quality |= APP_MEASUREMENT_QUALITY_GAP;
  }
  if (snapshot->status == ADXL345_STATUS_DATA_READY_STALLED)
  {
    quality |= APP_MEASUREMENT_QUALITY_GAP;
  }
  if ((source_quality & ADXL345_QUALITY_DROPPED) != 0U)
  {
    quality |= APP_MEASUREMENT_QUALITY_DROPPED;
  }
  if (((source_quality & ADXL345_QUALITY_TRANSPORT_ERROR) != 0U) ||
      app_measurement_transport_failed_adxl345(
          snapshot->last_transport_result))
  {
    quality |= APP_MEASUREMENT_QUALITY_TRANSPORT_ERROR;
  }
  if (((source_quality & ADXL345_QUALITY_CONFIGURATION_ERROR) != 0U) ||
      (snapshot->status == ADXL345_STATUS_WRONG_ID) ||
      (snapshot->status == ADXL345_STATUS_CONFIGURATION_MISMATCH))
  {
    quality |= APP_MEASUREMENT_QUALITY_CONFIGURATION_ERROR;
  }
  if (snapshot->status == ADXL345_STATUS_RECOVERY_REQUIRED)
  {
    quality |= APP_MEASUREMENT_QUALITY_RECOVERY_ACTIVE;
  }
  return quality;
}

static bool app_measurement_is_new_sequence(
    app_measurement_model_t *model,
    app_measurement_source_t source,
    uint32_t sequence)
{
  const size_t index = (size_t)source;
  if (!model->observed_sequence_valid[index] ||
      (model->observed_sequence[index] != sequence))
  {
    model->observed_sequence_valid[index] = true;
    model->observed_sequence[index] = sequence;
    return true;
  }
  return false;
}

static void app_measurement_accept_metadata(
    app_measurement_metadata_t *metadata,
    uint32_t sequence,
    uint32_t now_ms)
{
  metadata->sequence = sequence;
  metadata->sample_monotonic_ms = now_ms;
  metadata->age_ms = 0U;
  metadata->value_present = true;
  metadata->value_is_retained = false;
}

void app_measurement_model_initialize(app_measurement_model_t *model)
{
  if (model == NULL)
  {
    return;
  }
  (void)memset(model, 0, sizeof(*model));
  model->snapshot.schema_revision = APP_MEASUREMENT_SCHEMA_REVISION;
  for (size_t index = 0U; index < APP_MEASUREMENT_SOURCE_COUNT; ++index)
  {
    app_measurement_metadata_t *metadata = app_measurement_metadata(
        &model->snapshot, (app_measurement_source_t)index);
    metadata->source = (app_measurement_source_t)index;
    metadata->state = APP_MEASUREMENT_STATE_INVALID;
  }
}

bool app_measurement_refresh_snapshot(app_measurement_snapshot_t *snapshot,
                                      uint32_t now_ms)
{
  if ((snapshot == NULL) ||
      (snapshot->schema_revision != APP_MEASUREMENT_SCHEMA_REVISION))
  {
    return false;
  }

  snapshot->evaluated_monotonic_ms = now_ms;
  for (size_t index = 0U; index < APP_MEASUREMENT_SOURCE_COUNT; ++index)
  {
    const app_measurement_source_t source =
        (app_measurement_source_t)index;
    app_measurement_metadata_t *metadata =
        app_measurement_metadata(snapshot, source);
    if (!metadata->value_present)
    {
      metadata->age_ms = 0U;
      metadata->value_is_retained = false;
      if (metadata->state != APP_MEASUREMENT_STATE_OFFLINE)
      {
        metadata->state = APP_MEASUREMENT_STATE_INVALID;
      }
      continue;
    }

    metadata->age_ms = now_ms - metadata->sample_monotonic_ms;
    if (metadata->state == APP_MEASUREMENT_STATE_OFFLINE)
    {
      metadata->value_is_retained = true;
      continue;
    }
    if (metadata->age_ms <= app_measurement_fresh_limit(source))
    {
      metadata->state = APP_MEASUREMENT_STATE_FRESH;
      metadata->value_is_retained = false;
    }
    else
    {
      metadata->state = APP_MEASUREMENT_STATE_STALE;
      metadata->value_is_retained = true;
    }
  }
  return true;
}

bool app_measurement_update(app_measurement_model_t *model,
                            const app_measurement_inputs_t *inputs,
                            uint32_t now_ms)
{
  if ((model == NULL) || (inputs == NULL) ||
      (model->snapshot.schema_revision != APP_MEASUREMENT_SCHEMA_REVISION))
  {
    return false;
  }

  app_measurement_metadata_t *bme = &model->snapshot.bme280.metadata;
  app_measurement_metadata_t *veml = &model->snapshot.veml7700.metadata;
  app_measurement_metadata_t *adxl_sample =
      &model->snapshot.adxl345_sample.metadata;
  app_measurement_metadata_t *adxl_feature =
      &model->snapshot.adxl345_feature.metadata;

  bme->quality_flags = app_measurement_bme280_quality(&inputs->bme280);
  veml->quality_flags = app_measurement_veml7700_quality(&inputs->veml7700);
  adxl_sample->quality_flags = app_measurement_adxl345_quality(
      &inputs->adxl345, inputs->adxl345.sample.quality_flags);
  adxl_feature->quality_flags = app_measurement_adxl345_quality(
      &inputs->adxl345, inputs->adxl345.feature.quality_flags);

  bme->state = inputs->bme280.state == BME280_STATE_OFFLINE
                   ? APP_MEASUREMENT_STATE_OFFLINE
                   : APP_MEASUREMENT_STATE_INVALID;
  veml->state = inputs->veml7700.state == VEML7700_STATE_OFFLINE
                    ? APP_MEASUREMENT_STATE_OFFLINE
                    : APP_MEASUREMENT_STATE_INVALID;
  const app_measurement_state_t adxl_state =
      inputs->adxl345.state == ADXL345_STATE_OFFLINE
          ? APP_MEASUREMENT_STATE_OFFLINE
          : APP_MEASUREMENT_STATE_INVALID;
  adxl_sample->state = adxl_state;
  adxl_feature->state = adxl_state;

  const bme280_sample_t *bme_source = &inputs->bme280.sample;
  if ((bme_source->status == BME280_STATUS_VALID) &&
      (bme_source->valid_mask == BME280_SAMPLE_VALID_ALL) &&
      app_measurement_is_new_sequence(
          model, APP_MEASUREMENT_SOURCE_BME280, bme_source->sequence))
  {
    app_measurement_accept_metadata(bme, bme_source->sequence, now_ms);
    model->snapshot.bme280.temperature_centi_c =
        bme_source->temperature_centi_c;
    model->snapshot.bme280.pressure_pa = bme_source->pressure_pa;
    model->snapshot.bme280.humidity_milli_pct =
        bme_source->humidity_milli_pct;
  }

  const veml7700_sample_t *veml_source = &inputs->veml7700.sample;
  if (((veml_source->quality_flags & VEML7700_QUALITY_VALID) != 0U) &&
      app_measurement_is_new_sequence(
          model, APP_MEASUREMENT_SOURCE_VEML7700, veml_source->sequence))
  {
    app_measurement_accept_metadata(veml, veml_source->sequence, now_ms);
    model->snapshot.veml7700.illuminance_millilux =
        veml_source->illuminance_millilux;
  }

  const adxl345_sample_t *adxl_sample_source = &inputs->adxl345.sample;
  if (((adxl_sample_source->quality_flags & ADXL345_QUALITY_VALID) != 0U) &&
      app_measurement_is_new_sequence(
          model,
          APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE,
          adxl_sample_source->sequence))
  {
    app_measurement_accept_metadata(
        adxl_sample, adxl_sample_source->sequence, now_ms);
    for (size_t axis = 0U; axis < ADXL345_AXIS_COUNT; ++axis)
    {
      model->snapshot.adxl345_sample.acceleration_millig[axis] =
          adxl_sample_source->acceleration_millig[axis];
    }
  }

  const adxl345_feature_t *adxl_feature_source = &inputs->adxl345.feature;
  if (((adxl_feature_source->quality_flags & ADXL345_QUALITY_VALID) != 0U) &&
      (adxl_feature_source->sample_count ==
       ADXL345_FEATURE_WINDOW_SAMPLES) &&
      app_measurement_is_new_sequence(
          model,
          APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
          adxl_feature_source->sequence))
  {
    app_measurement_accept_metadata(
        adxl_feature, adxl_feature_source->sequence, now_ms);
    for (size_t axis = 0U; axis < ADXL345_AXIS_COUNT; ++axis)
    {
      model->snapshot.adxl345_feature.mean_millig[axis] =
          adxl_feature_source->mean_millig[axis];
      model->snapshot.adxl345_feature.rms_millig[axis] =
          adxl_feature_source->rms_millig[axis];
      model->snapshot.adxl345_feature.peak_abs_millig[axis] =
          adxl_feature_source->peak_abs_millig[axis];
    }
    model->snapshot.adxl345_feature.resultant_rms_millig =
        adxl_feature_source->resultant_rms_millig;
  }

  return app_measurement_refresh_snapshot(&model->snapshot, now_ms);
}

bool app_measurement_get_snapshot(const app_measurement_model_t *model,
                                  uint32_t now_ms,
                                  app_measurement_snapshot_t *snapshot)
{
  if ((model == NULL) || (snapshot == NULL))
  {
    return false;
  }
  *snapshot = model->snapshot;
  return app_measurement_refresh_snapshot(snapshot, now_ms);
}

size_t app_measurement_field_count(void)
{
  return APP_MEASUREMENT_FIELD_COUNT;
}

bool app_measurement_field_descriptor(
    size_t index,
    app_measurement_field_descriptor_t *descriptor)
{
  if ((index >= APP_MEASUREMENT_FIELD_COUNT) || (descriptor == NULL))
  {
    return false;
  }
  *descriptor = app_measurement_fields[index];
  return true;
}

static const app_measurement_field_descriptor_t *
app_measurement_find_field(app_measurement_field_id_t field_id)
{
  for (size_t index = 0U; index < APP_MEASUREMENT_FIELD_COUNT; ++index)
  {
    if (app_measurement_fields[index].field_id == field_id)
    {
      return &app_measurement_fields[index];
    }
  }
  return NULL;
}

bool app_measurement_read_field(
    const app_measurement_snapshot_t *snapshot,
    app_measurement_field_id_t field_id,
    int64_t *value,
    app_measurement_metadata_t *metadata)
{
  if ((snapshot == NULL) || (value == NULL))
  {
    return false;
  }
  const app_measurement_field_descriptor_t *descriptor =
      app_measurement_find_field(field_id);
  if (descriptor == NULL)
  {
    return false;
  }
  const app_measurement_metadata_t *source_metadata =
      app_measurement_const_metadata(snapshot, descriptor->source);
  if (metadata != NULL)
  {
    *metadata = *source_metadata;
  }
  if (!source_metadata->value_present)
  {
    return false;
  }

  switch (field_id)
  {
    case APP_MEASUREMENT_FIELD_BME280_TEMPERATURE:
      *value = snapshot->bme280.temperature_centi_c;
      return true;
    case APP_MEASUREMENT_FIELD_BME280_PRESSURE:
      *value = snapshot->bme280.pressure_pa;
      return true;
    case APP_MEASUREMENT_FIELD_BME280_HUMIDITY:
      *value = snapshot->bme280.humidity_milli_pct;
      return true;
    case APP_MEASUREMENT_FIELD_VEML7700_ILLUMINANCE:
      *value = snapshot->veml7700.illuminance_millilux;
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_ACCELERATION_X:
      *value = snapshot->adxl345_sample.acceleration_millig[0];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_ACCELERATION_Y:
      *value = snapshot->adxl345_sample.acceleration_millig[1];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_ACCELERATION_Z:
      *value = snapshot->adxl345_sample.acceleration_millig[2];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_MEAN_X:
      *value = snapshot->adxl345_feature.mean_millig[0];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_MEAN_Y:
      *value = snapshot->adxl345_feature.mean_millig[1];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_MEAN_Z:
      *value = snapshot->adxl345_feature.mean_millig[2];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_RMS_X:
      *value = snapshot->adxl345_feature.rms_millig[0];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_RMS_Y:
      *value = snapshot->adxl345_feature.rms_millig[1];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_RMS_Z:
      *value = snapshot->adxl345_feature.rms_millig[2];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_PEAK_X:
      *value = snapshot->adxl345_feature.peak_abs_millig[0];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_PEAK_Y:
      *value = snapshot->adxl345_feature.peak_abs_millig[1];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_PEAK_Z:
      *value = snapshot->adxl345_feature.peak_abs_millig[2];
      return true;
    case APP_MEASUREMENT_FIELD_ADXL345_RESULTANT_RMS:
      *value = snapshot->adxl345_feature.resultant_rms_millig;
      return true;
    default:
      return false;
  }
}
