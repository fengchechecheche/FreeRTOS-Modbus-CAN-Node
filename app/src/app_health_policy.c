#include "app_health_policy.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint32_t app_health_saturating_increment_u32(uint32_t value)
{
  return value == UINT32_MAX ? UINT32_MAX : value + 1U;
}

static uint8_t app_health_saturating_increment_u8(uint8_t value,
                                                  uint8_t limit)
{
  return value >= limit ? limit : (uint8_t)(value + 1U);
}

static bool app_health_counter_changed(uint32_t current, uint32_t previous)
{
  return current != previous;
}

static bool app_health_task_progressed(uint32_t current, uint32_t previous)
{
  if ((current == UINT32_MAX) && (previous == UINT32_MAX))
  {
    return true;
  }
  return current != previous;
}

static bool app_health_task_is_monitored(size_t index)
{
  return index != (size_t)APP_TASK_HEALTH;
}

static app_watchdog_feed_decision_t app_health_feed_for_state(
    app_health_state_t state)
{
  switch (state)
  {
    case APP_HEALTH_SERVICEABLE:
    case APP_HEALTH_DEGRADED:
    case APP_HEALTH_RECOVERY_REQUIRED:
      return APP_WATCHDOG_FEED_ALLOWED;
    case APP_HEALTH_BOOTSTRAP:
    case APP_HEALTH_RESET_REQUIRED:
    case APP_HEALTH_RESET_LOOP_LATCHED:
    default:
      return APP_WATCHDOG_FEED_WITHHELD;
  }
}

static void app_health_fill_decision(const app_health_policy_t *policy,
                                     uint32_t warning_mask,
                                     uint32_t stalled_task_mask,
                                     bool state_changed,
                                     bool publish_transition,
                                     app_health_decision_t *decision)
{
  decision->state = policy->state;
  decision->feed_decision = app_health_feed_for_state(policy->state);
  decision->warning_mask = warning_mask;
  decision->stalled_task_mask = stalled_task_mask;
  decision->epoch_count = policy->epoch_count;
  decision->state_transition_count = policy->state_transition_count;
  decision->recovery_attempts = policy->recovery_attempts;
  decision->state_changed = state_changed;
  decision->publish_transition = publish_transition;
}

void app_health_policy_initialize(app_health_policy_t *policy)
{
  if (policy == NULL)
  {
    return;
  }

  (void)memset(policy, 0, sizeof(*policy));
  policy->state = APP_HEALTH_BOOTSTRAP;
}

bool app_health_policy_evaluate(app_health_policy_t *policy,
                                const app_health_input_t *input,
                                app_health_decision_t *decision)
{
  if ((policy == NULL) || (input == NULL) || (decision == NULL))
  {
    return false;
  }

  policy->epoch_count = app_health_saturating_increment_u32(
      policy->epoch_count);

  if (!policy->initialized)
  {
    policy->previous_input = *input;
    policy->initialized = true;
    app_health_fill_decision(policy, 0U, 0U, false, false, decision);
    return true;
  }

  const app_health_state_t previous_state = policy->state;
  uint32_t warning_mask = 0U;
  uint32_t stalled_task_mask = 0U;
  bool reset_required = false;

  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    if (app_health_task_is_monitored(index))
    {
      const uint32_t current_release = input->task_release_count[index];
      const uint32_t previous_release =
          policy->previous_input.task_release_count[index];
      if (app_health_task_progressed(current_release, previous_release))
      {
        policy->task_stall_epochs[index] = 0U;
      }
      else
      {
        policy->task_stall_epochs[index] =
            app_health_saturating_increment_u8(
                policy->task_stall_epochs[index],
                APP_HEALTH_STALL_EPOCH_LIMIT);
      }

      if (policy->task_stall_epochs[index] != 0U)
      {
        warning_mask |= APP_HEALTH_WARNING_TASK_STALL;
        stalled_task_mask |= UINT32_C(1) << index;
      }
      if (policy->task_stall_epochs[index] >=
          APP_HEALTH_STALL_EPOCH_LIMIT)
      {
        reset_required = true;
      }
    }

    if (app_health_counter_changed(
            input->task_deadline_miss_count[index],
            policy->previous_input.task_deadline_miss_count[index]))
    {
      warning_mask |= APP_HEALTH_WARNING_DEADLINE_MISS;
    }
    if (app_health_counter_changed(
            input->task_budget_overrun_count[index],
            policy->previous_input.task_budget_overrun_count[index]))
    {
      warning_mask |= APP_HEALTH_WARNING_BUDGET_OVERRUN;
    }
    if ((input->task_release_count[index] == UINT32_MAX) ||
        (input->task_deadline_miss_count[index] == UINT32_MAX) ||
        (input->task_budget_overrun_count[index] == UINT32_MAX))
    {
      warning_mask |= APP_HEALTH_WARNING_COUNTER_SATURATED;
    }
  }

  if (app_health_counter_changed(
          input->queue_dropped_count,
          policy->previous_input.queue_dropped_count) ||
      ((input->queue_depth != 0U) &&
       ((input->queue_current_pending >= input->queue_depth) ||
        ((input->queue_maximum_pending >= input->queue_depth) &&
         app_health_counter_changed(
             input->queue_maximum_pending,
             policy->previous_input.queue_maximum_pending)))))
  {
    warning_mask |= APP_HEALTH_WARNING_QUEUE_PRESSURE;
  }
  if (app_health_counter_changed(
          input->snapshot_contention_count,
          policy->previous_input.snapshot_contention_count))
  {
    warning_mask |= APP_HEALTH_WARNING_SNAPSHOT_CONTENTION;
  }
  if (app_health_counter_changed(
          input->rs485_error_count,
          policy->previous_input.rs485_error_count))
  {
    warning_mask |= APP_HEALTH_WARNING_RS485_ERROR;
  }
  if (input->sensor_unavailable_mask != 0U)
  {
    warning_mask |= APP_HEALTH_WARNING_SENSOR_UNAVAILABLE;
  }
  if (input->sensor_stale_mask != 0U)
  {
    warning_mask |= APP_HEALTH_WARNING_SENSOR_STALE;
  }
  if (input->sensor_recovery_mask != 0U)
  {
    warning_mask |= APP_HEALTH_WARNING_SENSOR_RECOVERY;
  }

  switch (input->recovery_result)
  {
    case APP_HEALTH_RECOVERY_SUCCEEDED:
      policy->recovery_attempts = 0U;
      break;
    case APP_HEALTH_RECOVERY_FAILED:
      policy->recovery_attempts = app_health_saturating_increment_u8(
          policy->recovery_attempts, APP_HEALTH_RECOVERY_ATTEMPT_LIMIT);
      warning_mask |= APP_HEALTH_WARNING_RECOVERY_ACTIVE;
      if (policy->recovery_attempts >= APP_HEALTH_RECOVERY_ATTEMPT_LIMIT)
      {
        warning_mask |= APP_HEALTH_WARNING_RECOVERY_EXHAUSTED;
        reset_required = true;
      }
      break;
    case APP_HEALTH_RECOVERY_REQUESTED:
      warning_mask |= APP_HEALTH_WARNING_RECOVERY_ACTIVE;
      break;
    case APP_HEALTH_RECOVERY_NONE:
    default:
      break;
  }

  if ((policy->recovery_attempts != 0U) &&
      (input->recovery_result != APP_HEALTH_RECOVERY_SUCCEEDED))
  {
    warning_mask |= APP_HEALTH_WARNING_RECOVERY_ACTIVE;
  }

  if (input->reset_loop_latched)
  {
    policy->state = APP_HEALTH_RESET_LOOP_LATCHED;
  }
  else if (reset_required)
  {
    policy->state = APP_HEALTH_RESET_REQUIRED;
  }
  else if ((input->recovery_result == APP_HEALTH_RECOVERY_REQUESTED) ||
           (policy->recovery_attempts != 0U))
  {
    policy->state = APP_HEALTH_RECOVERY_REQUIRED;
  }
  else if (warning_mask != 0U)
  {
    policy->state = APP_HEALTH_DEGRADED;
  }
  else
  {
    policy->state = APP_HEALTH_SERVICEABLE;
  }

  const bool state_changed = policy->state != previous_state;
  const bool publish_transition =
      state_changed && (previous_state != APP_HEALTH_BOOTSTRAP);
  if (state_changed)
  {
    policy->state_transition_count = app_health_saturating_increment_u32(
        policy->state_transition_count);
  }

  policy->previous_input = *input;
  app_health_fill_decision(policy,
                           warning_mask,
                           stalled_task_mask,
                           state_changed,
                           publish_transition,
                           decision);
  return true;
}

uint16_t app_health_state_event_code(app_health_state_t state)
{
  switch (state)
  {
    case APP_HEALTH_DEGRADED:
      return APP_HEALTH_EVENT_CODE_DEGRADED;
    case APP_HEALTH_RECOVERY_REQUIRED:
      return APP_HEALTH_EVENT_CODE_RECOVERY_REQUIRED;
    case APP_HEALTH_RESET_REQUIRED:
      return APP_HEALTH_EVENT_CODE_FEED_WITHHELD;
    case APP_HEALTH_RESET_LOOP_LATCHED:
      return APP_HEALTH_EVENT_CODE_RESET_LOOP_LATCHED;
    case APP_HEALTH_SERVICEABLE:
      return APP_HEALTH_EVENT_CODE_RECOVERED;
    case APP_HEALTH_BOOTSTRAP:
    default:
      return 0U;
  }
}
