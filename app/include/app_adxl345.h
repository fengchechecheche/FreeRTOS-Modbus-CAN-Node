#ifndef APP_ADXL345_H
#define APP_ADXL345_H

#include <stdbool.h>
#include <stdint.h>

#include "adxl345.h"

#define APP_ADXL345_SPI_TIMEOUT_MS UINT32_C(5)

typedef struct
{
  adxl345_state_t state;
  adxl345_status_t status;
  adxl345_status_t last_error_status;
  adxl345_transport_result_t last_transport_result;
  adxl345_sample_t sample;
  adxl345_feature_t feature;
  uint32_t feature_window_count;
  uint32_t transaction_count;
  uint32_t error_count;
  uint32_t recovery_request_count;
  uint32_t recovery_success_count;
  uint32_t irq_event_count;
  uint32_t dropped_sample_lower_bound;
} app_adxl345_snapshot_t;

typedef struct
{
  bool attempted;
  bool valid;
  uint8_t power_ctl;
  uint8_t int_enable;
  uint8_t int_map;
  uint8_t int_source;
} app_adxl345_register_diagnostic_t;

void app_adxl345_initialize(void);
void app_adxl345_service(uint32_t now_ms,
                         uint32_t data_ready_event_count);
/* Owner-context diagnostic snapshot; cross-task users read app_measurement. */
bool app_adxl345_get_snapshot(app_adxl345_snapshot_t *snapshot);
void app_adxl345_capture_register_diagnostic_once(void);
bool app_adxl345_get_register_diagnostic(
    app_adxl345_register_diagnostic_t *diagnostic);

#endif
