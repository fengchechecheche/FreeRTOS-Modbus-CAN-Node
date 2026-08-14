#ifndef APP_RTOS_H
#define APP_RTOS_H

#include <stdbool.h>
#include <stdint.h>

#include "app_task_model.h"

typedef enum
{
  APP_RTOS_OK = 0,
  APP_RTOS_ERROR = 1
} app_rtos_status_t;

typedef enum
{
  APP_RTOS_FAULT_NONE = 0U,
  APP_RTOS_FAULT_MODEL = 0x5301U,
  APP_RTOS_FAULT_TASK_CREATE = 0x5302U,
  APP_RTOS_FAULT_CYCLE = 0x5303U,
  APP_RTOS_FAULT_ASSERT = 0x5310U,
  APP_RTOS_FAULT_STACK_OVERFLOW = 0x5320U,
  APP_RTOS_FAULT_SCHEDULER_RETURN = 0x5330U
} app_rtos_fault_t;

typedef struct
{
  uint32_t release_count;
  uint32_t missed_release_count;
  uint32_t deadline_miss_count;
  uint32_t budget_overrun_count;
} app_rtos_health_snapshot_t;

typedef struct
{
  uint32_t configured_words;
  uint32_t minimum_free_words;
  bool measured;
} app_rtos_task_resource_t;

typedef struct
{
  app_rtos_task_resource_t task[APP_TASK_COUNT];
} app_rtos_resource_snapshot_t;

typedef struct
{
  uint32_t sample_count;
  uint32_t minimum_cycles;
  uint32_t maximum_cycles;
  uint32_t last_cycles;
  bool measured;
  bool enabled;
} app_rtos_irq_latency_snapshot_t;

app_rtos_status_t app_rtos_initialize(void);
void app_rtos_get_health_snapshot(app_rtos_health_snapshot_t *snapshot);
void app_rtos_get_resource_snapshot(app_rtos_resource_snapshot_t *snapshot);
void app_rtos_get_irq_latency_snapshot(
    app_rtos_irq_latency_snapshot_t *snapshot);
uint32_t app_rtos_fault_code(void);
_Noreturn void app_rtos_fail_stop(uint32_t fault_code);

#endif
