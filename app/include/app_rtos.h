#ifndef APP_RTOS_H
#define APP_RTOS_H

#include <stdbool.h>
#include <stdint.h>

#include "app_health_policy.h"
#include "app_measurement.h"
#include "app_reset_reason.h"
#include "app_sensor_monitor.h"
#include "app_task_model.h"
#include "app_transport_policy.h"

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
  APP_RTOS_FAULT_QUEUE_CREATE = 0x5304U,
  APP_RTOS_FAULT_MUTEX_CREATE = 0x5305U,
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
} app_rtos_task_health_t;

typedef struct
{
  uint32_t release_count;
  uint32_t missed_release_count;
  uint32_t deadline_miss_count;
  uint32_t budget_overrun_count;
  app_rtos_task_health_t task[APP_TASK_COUNT];
  app_health_decision_t decision;
  uint32_t rs485_error_count;
} app_rtos_health_snapshot_t;

typedef struct
{
  app_transport_counters_t counters;
  uint32_t current_pending;
  uint32_t maximum_pending;
  uint32_t depth;
} app_rtos_transport_snapshot_t;

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
/* The queue and snapshot APIs below are task-context only. */
bool app_rtos_publish_diagnostic_event(const app_transport_event_t *event);
bool app_rtos_get_health_snapshot(app_rtos_health_snapshot_t *snapshot);
bool app_rtos_get_resource_snapshot(app_rtos_resource_snapshot_t *snapshot);
bool app_rtos_get_irq_latency_snapshot(
    app_rtos_irq_latency_snapshot_t *snapshot);
bool app_rtos_get_measurement_snapshot(
    app_measurement_snapshot_t *snapshot);
bool app_rtos_get_sensor_monitor_snapshot(
    app_sensor_monitor_snapshot_t *snapshot);
bool app_rtos_get_transport_counters(app_transport_counters_t *counters);
bool app_rtos_get_transport_snapshot(app_rtos_transport_snapshot_t *snapshot);
bool app_rtos_get_reset_reason(app_reset_decoded_t *decoded);
uint32_t app_rtos_fault_code(void);
_Noreturn void app_rtos_fail_stop(uint32_t fault_code);

#endif
