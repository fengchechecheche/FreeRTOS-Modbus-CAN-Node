#ifndef APP_HEALTH_POLICY_H
#define APP_HEALTH_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include "app_task_model.h"

#define APP_HEALTH_STALL_EPOCH_LIMIT (2U)
#define APP_HEALTH_RECOVERY_ATTEMPT_LIMIT (3U)
#define APP_HEALTH_EVENT_SOURCE (1U)
#define APP_HEALTH_EVENT_CODE_DEGRADED (UINT16_C(0x0101))
#define APP_HEALTH_EVENT_CODE_RECOVERY_REQUIRED (UINT16_C(0x0102))
#define APP_HEALTH_EVENT_CODE_FEED_WITHHELD (UINT16_C(0x0103))
#define APP_HEALTH_EVENT_CODE_RESET_LOOP_LATCHED (UINT16_C(0x0104))
#define APP_HEALTH_EVENT_CODE_RECOVERED (UINT16_C(0x0105))

#define APP_HEALTH_WARNING_TASK_STALL (UINT32_C(1) << 0)
#define APP_HEALTH_WARNING_DEADLINE_MISS (UINT32_C(1) << 1)
#define APP_HEALTH_WARNING_BUDGET_OVERRUN (UINT32_C(1) << 2)
#define APP_HEALTH_WARNING_QUEUE_PRESSURE (UINT32_C(1) << 3)
#define APP_HEALTH_WARNING_SNAPSHOT_CONTENTION (UINT32_C(1) << 4)
#define APP_HEALTH_WARNING_RS485_ERROR (UINT32_C(1) << 5)
#define APP_HEALTH_WARNING_COUNTER_SATURATED (UINT32_C(1) << 6)
#define APP_HEALTH_WARNING_RECOVERY_ACTIVE (UINT32_C(1) << 7)
#define APP_HEALTH_WARNING_RECOVERY_EXHAUSTED (UINT32_C(1) << 8)
#define APP_HEALTH_WARNING_SENSOR_UNAVAILABLE (UINT32_C(1) << 9)
#define APP_HEALTH_WARNING_SENSOR_STALE (UINT32_C(1) << 10)
#define APP_HEALTH_WARNING_SENSOR_RECOVERY (UINT32_C(1) << 11)

typedef enum
{
  APP_HEALTH_BOOTSTRAP = 0,
  APP_HEALTH_SERVICEABLE,
  APP_HEALTH_DEGRADED,
  APP_HEALTH_RECOVERY_REQUIRED,
  APP_HEALTH_RESET_REQUIRED,
  APP_HEALTH_RESET_LOOP_LATCHED
} app_health_state_t;

typedef enum
{
  APP_WATCHDOG_FEED_WITHHELD = 0,
  APP_WATCHDOG_FEED_ALLOWED
} app_watchdog_feed_decision_t;

typedef enum
{
  APP_HEALTH_RECOVERY_NONE = 0,
  APP_HEALTH_RECOVERY_REQUESTED,
  APP_HEALTH_RECOVERY_SUCCEEDED,
  APP_HEALTH_RECOVERY_FAILED
} app_health_recovery_result_t;

typedef struct
{
  uint32_t task_release_count[APP_TASK_COUNT];
  uint32_t task_deadline_miss_count[APP_TASK_COUNT];
  uint32_t task_budget_overrun_count[APP_TASK_COUNT];
  uint32_t queue_dropped_count;
  uint32_t queue_current_pending;
  uint32_t queue_maximum_pending;
  uint32_t queue_depth;
  uint32_t snapshot_contention_count;
  uint32_t rs485_error_count;
  uint32_t sensor_unavailable_mask;
  uint32_t sensor_stale_mask;
  uint32_t sensor_recovery_mask;
  app_health_recovery_result_t recovery_result;
  bool reset_loop_latched;
} app_health_input_t;

typedef struct
{
  app_health_state_t state;
  app_watchdog_feed_decision_t feed_decision;
  uint32_t warning_mask;
  uint32_t stalled_task_mask;
  uint32_t epoch_count;
  uint32_t state_transition_count;
  uint8_t recovery_attempts;
  bool state_changed;
  bool publish_transition;
} app_health_decision_t;

typedef struct
{
  app_health_input_t previous_input;
  uint8_t task_stall_epochs[APP_TASK_COUNT];
  uint8_t recovery_attempts;
  app_health_state_t state;
  uint32_t epoch_count;
  uint32_t state_transition_count;
  bool initialized;
} app_health_policy_t;

void app_health_policy_initialize(app_health_policy_t *policy);
bool app_health_policy_evaluate(app_health_policy_t *policy,
                                const app_health_input_t *input,
                                app_health_decision_t *decision);
uint16_t app_health_state_event_code(app_health_state_t state);

#endif
