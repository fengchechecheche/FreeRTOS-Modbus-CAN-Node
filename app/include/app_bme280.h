#ifndef APP_BME280_H
#define APP_BME280_H

#include <stdbool.h>
#include <stdint.h>

#include "bme280.h"

#define APP_BME280_SPI_TIMEOUT_MS UINT32_C(5)

typedef struct
{
  bme280_state_t state;
  bme280_status_t status;
  bme280_transport_result_t last_transport_result;
  bme280_sample_t sample;
  uint32_t transaction_count;
  uint32_t error_count;
  uint32_t recovery_request_count;
  uint32_t recovery_success_count;
} app_bme280_snapshot_t;

void app_bme280_initialize(void);
void app_bme280_service(uint32_t now_ms);
/* Owner-context only until P5-S4-T04 defines the shared sample schema. */
bool app_bme280_get_snapshot(app_bme280_snapshot_t *snapshot);

#endif
