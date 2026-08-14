#ifndef APP_MEASUREMENT_H
#define APP_MEASUREMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_adxl345.h"
#include "app_bme280.h"
#include "app_veml7700.h"

#define APP_MEASUREMENT_SCHEMA_REVISION UINT32_C(1)
#define APP_MEASUREMENT_BME280_FRESH_MS UINT32_C(2500)
#define APP_MEASUREMENT_VEML7700_FRESH_MS UINT32_C(3000)
#define APP_MEASUREMENT_ADXL345_SAMPLE_FRESH_MS UINT32_C(200)
#define APP_MEASUREMENT_ADXL345_FEATURE_FRESH_MS UINT32_C(2500)

#define APP_MEASUREMENT_QUALITY_UNCALIBRATED (UINT32_C(1) << 0)
#define APP_MEASUREMENT_QUALITY_RANGE_WARNING (UINT32_C(1) << 1)
#define APP_MEASUREMENT_QUALITY_SATURATED (UINT32_C(1) << 2)
#define APP_MEASUREMENT_QUALITY_GAP (UINT32_C(1) << 3)
#define APP_MEASUREMENT_QUALITY_DROPPED (UINT32_C(1) << 4)
#define APP_MEASUREMENT_QUALITY_TRANSPORT_ERROR (UINT32_C(1) << 5)
#define APP_MEASUREMENT_QUALITY_CONFIGURATION_ERROR (UINT32_C(1) << 6)
#define APP_MEASUREMENT_QUALITY_RECOVERY_ACTIVE (UINT32_C(1) << 7)

typedef enum
{
  APP_MEASUREMENT_SOURCE_BME280 = 0,
  APP_MEASUREMENT_SOURCE_VEML7700,
  APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE,
  APP_MEASUREMENT_SOURCE_ADXL345_FEATURE,
  APP_MEASUREMENT_SOURCE_COUNT
} app_measurement_source_t;

typedef enum
{
  APP_MEASUREMENT_STATE_INVALID = 0,
  APP_MEASUREMENT_STATE_FRESH,
  APP_MEASUREMENT_STATE_STALE,
  APP_MEASUREMENT_STATE_OFFLINE
} app_measurement_state_t;

typedef enum
{
  APP_MEASUREMENT_UNIT_CENTI_DEG_C = 0,
  APP_MEASUREMENT_UNIT_PA,
  APP_MEASUREMENT_UNIT_MILLI_PERCENT_RH,
  APP_MEASUREMENT_UNIT_MILLILUX,
  APP_MEASUREMENT_UNIT_MILLIG
} app_measurement_unit_t;

typedef enum
{
  APP_MEASUREMENT_FIELD_BME280_TEMPERATURE = 0x0101,
  APP_MEASUREMENT_FIELD_BME280_PRESSURE = 0x0102,
  APP_MEASUREMENT_FIELD_BME280_HUMIDITY = 0x0103,
  APP_MEASUREMENT_FIELD_VEML7700_ILLUMINANCE = 0x0201,
  APP_MEASUREMENT_FIELD_ADXL345_ACCELERATION_X = 0x0301,
  APP_MEASUREMENT_FIELD_ADXL345_ACCELERATION_Y = 0x0302,
  APP_MEASUREMENT_FIELD_ADXL345_ACCELERATION_Z = 0x0303,
  APP_MEASUREMENT_FIELD_ADXL345_MEAN_X = 0x0311,
  APP_MEASUREMENT_FIELD_ADXL345_MEAN_Y = 0x0312,
  APP_MEASUREMENT_FIELD_ADXL345_MEAN_Z = 0x0313,
  APP_MEASUREMENT_FIELD_ADXL345_RMS_X = 0x0321,
  APP_MEASUREMENT_FIELD_ADXL345_RMS_Y = 0x0322,
  APP_MEASUREMENT_FIELD_ADXL345_RMS_Z = 0x0323,
  APP_MEASUREMENT_FIELD_ADXL345_PEAK_X = 0x0331,
  APP_MEASUREMENT_FIELD_ADXL345_PEAK_Y = 0x0332,
  APP_MEASUREMENT_FIELD_ADXL345_PEAK_Z = 0x0333,
  APP_MEASUREMENT_FIELD_ADXL345_RESULTANT_RMS = 0x0341
} app_measurement_field_id_t;

typedef struct
{
  app_measurement_source_t source;
  app_measurement_state_t state;
  uint32_t sequence;
  uint32_t sample_monotonic_ms;
  uint32_t age_ms;
  uint32_t quality_flags;
  bool value_present;
  bool value_is_retained;
} app_measurement_metadata_t;

typedef struct
{
  app_measurement_metadata_t metadata;
  int32_t temperature_centi_c;
  uint32_t pressure_pa;
  uint32_t humidity_milli_pct;
} app_measurement_bme280_t;

typedef struct
{
  app_measurement_metadata_t metadata;
  uint32_t illuminance_millilux;
} app_measurement_veml7700_t;

typedef struct
{
  app_measurement_metadata_t metadata;
  int32_t acceleration_millig[ADXL345_AXIS_COUNT];
} app_measurement_adxl345_sample_t;

typedef struct
{
  app_measurement_metadata_t metadata;
  int32_t mean_millig[ADXL345_AXIS_COUNT];
  uint32_t rms_millig[ADXL345_AXIS_COUNT];
  uint32_t peak_abs_millig[ADXL345_AXIS_COUNT];
  uint32_t resultant_rms_millig;
} app_measurement_adxl345_feature_t;

typedef struct
{
  uint32_t schema_revision;
  uint32_t evaluated_monotonic_ms;
  app_measurement_bme280_t bme280;
  app_measurement_veml7700_t veml7700;
  app_measurement_adxl345_sample_t adxl345_sample;
  app_measurement_adxl345_feature_t adxl345_feature;
} app_measurement_snapshot_t;

typedef struct
{
  app_bme280_snapshot_t bme280;
  app_veml7700_snapshot_t veml7700;
  app_adxl345_snapshot_t adxl345;
} app_measurement_inputs_t;

typedef struct
{
  app_measurement_snapshot_t snapshot;
  uint32_t observed_sequence[APP_MEASUREMENT_SOURCE_COUNT];
  bool observed_sequence_valid[APP_MEASUREMENT_SOURCE_COUNT];
} app_measurement_model_t;

typedef struct
{
  app_measurement_field_id_t field_id;
  app_measurement_source_t source;
  app_measurement_unit_t unit;
} app_measurement_field_descriptor_t;

void app_measurement_model_initialize(app_measurement_model_t *model);
bool app_measurement_update(app_measurement_model_t *model,
                            const app_measurement_inputs_t *inputs,
                            uint32_t now_ms);
bool app_measurement_get_snapshot(const app_measurement_model_t *model,
                                  uint32_t now_ms,
                                  app_measurement_snapshot_t *snapshot);
bool app_measurement_refresh_snapshot(app_measurement_snapshot_t *snapshot,
                                      uint32_t now_ms);
size_t app_measurement_field_count(void);
bool app_measurement_field_descriptor(
    size_t index,
    app_measurement_field_descriptor_t *descriptor);
bool app_measurement_read_field(
    const app_measurement_snapshot_t *snapshot,
    app_measurement_field_id_t field_id,
    int64_t *value,
    app_measurement_metadata_t *metadata);

#endif
