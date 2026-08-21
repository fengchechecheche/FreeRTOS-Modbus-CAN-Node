#ifndef APP_SOAK_DIAGNOSTIC_H
#define APP_SOAK_DIAGNOSTIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_task_model.h"

#define APP_SOAK_DIAGNOSTIC_SCHEMA_REVISION UINT32_C(1)
#define APP_SOAK_DIAGNOSTIC_SOURCE_COUNT (4U)
#define APP_SOAK_DIAGNOSTIC_LINE_CAPACITY (1024U)

typedef struct
{
  uint32_t release;
  uint32_t missed;
  uint32_t deadline_miss;
  uint32_t budget_overrun;
  uint32_t configured_words;
  uint32_t minimum_free_words;
  bool measured;
} app_soak_task_t;

typedef struct
{
  uint32_t state;
  uint32_t sequence;
  uint32_t fault_count;
  uint32_t recovery_count;
} app_soak_sensor_t;

typedef struct
{
  uint32_t now_ms;
  uint32_t boot_count;
  app_soak_task_t task[APP_TASK_COUNT];
  uint32_t queue_current;
  uint32_t queue_maximum;
  uint32_t queue_depth;
  uint32_t queue_dropped;
  uint32_t queue_drained;
  uint32_t health_state;
  uint32_t health_warning_mask;
  uint32_t health_stalled_mask;
  uint32_t watchdog_feed;
  uint32_t fault_code;
  uint32_t reset_primary;
  uint32_t reset_raw_flags;
  bool reset_loop;
  uint32_t rs485_accepted;
  uint32_t rs485_error_count;
  uint32_t can_state;
  uint32_t can_pending;
  uint32_t can_capacity;
  uint32_t can_maximum_pending;
  uint32_t can_event_dropped;
  uint32_t can_hal_busy;
  uint32_t can_bus_off;
  uint32_t can_recovery_attempts;
  app_soak_sensor_t sensor[APP_SOAK_DIAGNOSTIC_SOURCE_COUNT];
} app_soak_diagnostic_snapshot_t;

bool app_soak_diagnostic_format(
    const app_soak_diagnostic_snapshot_t *snapshot,
    char *buffer,
    size_t capacity,
    size_t *length);

#endif
