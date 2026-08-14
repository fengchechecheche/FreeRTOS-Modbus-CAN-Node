#ifndef APP_VEML7700_H
#define APP_VEML7700_H

#include <stdbool.h>
#include <stdint.h>

#include "veml7700.h"

#define APP_VEML7700_I2C_TIMEOUT_MS UINT32_C(5)

typedef struct
{
  veml7700_state_t state;
  veml7700_status_t status;
  veml7700_status_t last_error_status;
  veml7700_transport_result_t last_transport_result;
  veml7700_sample_t sample;
  uint32_t transaction_count;
  uint32_t error_count;
  uint32_t range_change_count;
  uint32_t recovery_request_count;
  uint32_t recovery_success_count;
} app_veml7700_snapshot_t;

void app_veml7700_initialize(void);
void app_veml7700_service(uint32_t now_ms);
/* Owner-context only until P5-S4-T04 defines the shared sample schema. */
bool app_veml7700_get_snapshot(app_veml7700_snapshot_t *snapshot);

#endif
