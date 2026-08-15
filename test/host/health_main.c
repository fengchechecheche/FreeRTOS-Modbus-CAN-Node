#include "app_health_policy.h"
#include "app_reset_reason.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define CHECK(condition)          \
  do                              \
  {                               \
    if (!(condition))             \
    {                             \
      return EXIT_FAILURE;        \
    }                             \
  } while (false)

static void advance_monitored_tasks(app_health_input_t *input)
{
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    if (index != (size_t)APP_TASK_HEALTH)
    {
      ++input->task_release_count[index];
    }
  }
}

static int test_bootstrap_and_serviceable(void)
{
  app_health_policy_t policy;
  app_health_input_t input = {0};
  app_health_decision_t decision;
  input.queue_depth = 8U;
  app_health_policy_initialize(&policy);

  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  CHECK(decision.state == APP_HEALTH_BOOTSTRAP);
  CHECK(decision.feed_decision == APP_WATCHDOG_FEED_WITHHELD);
  CHECK(!decision.publish_transition);

  advance_monitored_tasks(&input);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  CHECK(decision.state == APP_HEALTH_SERVICEABLE);
  CHECK(decision.feed_decision == APP_WATCHDOG_FEED_ALLOWED);
  CHECK(decision.state_changed);
  CHECK(!decision.publish_transition);
  CHECK(app_health_state_event_code(decision.state) ==
        APP_HEALTH_EVENT_CODE_RECOVERED);
  return EXIT_SUCCESS;
}

static int test_stall_requires_two_epochs(void)
{
  app_health_policy_t policy;
  app_health_input_t input = {0};
  app_health_decision_t decision;
  input.queue_depth = 8U;
  app_health_policy_initialize(&policy);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));

  advance_monitored_tasks(&input);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  advance_monitored_tasks(&input);
  --input.task_release_count[APP_TASK_PROTOCOL];
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  CHECK(decision.state == APP_HEALTH_DEGRADED);
  CHECK(decision.feed_decision == APP_WATCHDOG_FEED_ALLOWED);
  CHECK((decision.stalled_task_mask &
         (UINT32_C(1) << APP_TASK_PROTOCOL)) != 0U);

  advance_monitored_tasks(&input);
  --input.task_release_count[APP_TASK_PROTOCOL];
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  CHECK(decision.state == APP_HEALTH_RESET_REQUIRED);
  CHECK(decision.feed_decision == APP_WATCHDOG_FEED_WITHHELD);

  advance_monitored_tasks(&input);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  CHECK(decision.state == APP_HEALTH_SERVICEABLE);
  CHECK(decision.feed_decision == APP_WATCHDOG_FEED_ALLOWED);
  return EXIT_SUCCESS;
}

static int test_warning_storm_does_not_force_reset(void)
{
  app_health_policy_t policy;
  app_health_input_t input = {0};
  app_health_decision_t decision;
  input.queue_depth = 8U;
  app_health_policy_initialize(&policy);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  advance_monitored_tasks(&input);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));

  for (uint32_t epoch = 0U; epoch < 8U; ++epoch)
  {
    advance_monitored_tasks(&input);
    ++input.task_deadline_miss_count[APP_TASK_PROTOCOL];
    ++input.task_budget_overrun_count[APP_TASK_CAN];
    ++input.queue_dropped_count;
    input.queue_current_pending = 8U;
    input.queue_maximum_pending = 8U;
    ++input.snapshot_contention_count;
    ++input.rs485_error_count;
    CHECK(app_health_policy_evaluate(&policy, &input, &decision));
    CHECK(decision.state == APP_HEALTH_DEGRADED);
    CHECK(decision.feed_decision == APP_WATCHDOG_FEED_ALLOWED);
    CHECK(decision.stalled_task_mask == 0U);
  }
  return EXIT_SUCCESS;
}

static int test_recovery_is_bounded(void)
{
  app_health_policy_t policy;
  app_health_input_t input = {0};
  app_health_decision_t decision;
  input.queue_depth = 8U;
  app_health_policy_initialize(&policy);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  advance_monitored_tasks(&input);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));

  input.recovery_result = APP_HEALTH_RECOVERY_REQUESTED;
  advance_monitored_tasks(&input);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  CHECK(decision.state == APP_HEALTH_RECOVERY_REQUIRED);
  CHECK(decision.feed_decision == APP_WATCHDOG_FEED_ALLOWED);

  input.recovery_result = APP_HEALTH_RECOVERY_FAILED;
  for (uint8_t attempt = 1U; attempt <= APP_HEALTH_RECOVERY_ATTEMPT_LIMIT;
       ++attempt)
  {
    advance_monitored_tasks(&input);
    CHECK(app_health_policy_evaluate(&policy, &input, &decision));
    CHECK(decision.recovery_attempts == attempt);
  }
  CHECK(decision.state == APP_HEALTH_RESET_REQUIRED);
  CHECK(decision.feed_decision == APP_WATCHDOG_FEED_WITHHELD);

  input.recovery_result = APP_HEALTH_RECOVERY_SUCCEEDED;
  advance_monitored_tasks(&input);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  CHECK(decision.state == APP_HEALTH_SERVICEABLE);
  CHECK(decision.recovery_attempts == 0U);
  return EXIT_SUCCESS;
}

static int test_sensor_fault_is_locally_degraded(void)
{
  app_health_policy_t policy;
  app_health_input_t input = {0};
  app_health_decision_t decision;
  input.queue_depth = 8U;
  app_health_policy_initialize(&policy);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  advance_monitored_tasks(&input);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));

  input.sensor_unavailable_mask = UINT32_C(1) << 1U;
  input.sensor_stale_mask = UINT32_C(1) << 2U;
  input.sensor_recovery_mask = UINT32_C(1) << 1U;
  for (uint32_t epoch = 0U; epoch < 4U; ++epoch)
  {
    advance_monitored_tasks(&input);
    CHECK(app_health_policy_evaluate(&policy, &input, &decision));
    CHECK(decision.state == APP_HEALTH_DEGRADED);
    CHECK(decision.feed_decision == APP_WATCHDOG_FEED_ALLOWED);
    CHECK(decision.recovery_attempts == 0U);
    CHECK(decision.stalled_task_mask == 0U);
    CHECK((decision.warning_mask &
           APP_HEALTH_WARNING_SENSOR_UNAVAILABLE) != 0U);
    CHECK((decision.warning_mask & APP_HEALTH_WARNING_SENSOR_STALE) != 0U);
    CHECK((decision.warning_mask &
           APP_HEALTH_WARNING_SENSOR_RECOVERY) != 0U);
  }

  input.sensor_unavailable_mask = 0U;
  input.sensor_stale_mask = 0U;
  input.sensor_recovery_mask = 0U;
  advance_monitored_tasks(&input);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  CHECK(decision.state == APP_HEALTH_SERVICEABLE);
  CHECK(decision.feed_decision == APP_WATCHDOG_FEED_ALLOWED);
  return EXIT_SUCCESS;
}

static int test_saturated_progress_counter_is_not_false_stall(void)
{
  app_health_policy_t policy;
  app_health_input_t input = {0};
  app_health_decision_t decision;
  input.queue_depth = 8U;
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    input.task_release_count[index] = UINT32_MAX;
  }
  app_health_policy_initialize(&policy);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  CHECK(decision.state == APP_HEALTH_DEGRADED);
  CHECK((decision.warning_mask &
         APP_HEALTH_WARNING_COUNTER_SATURATED) != 0U);
  CHECK(decision.stalled_task_mask == 0U);
  CHECK(decision.feed_decision == APP_WATCHDOG_FEED_ALLOWED);
  return EXIT_SUCCESS;
}

static int test_reset_reason_and_loop_record(void)
{
  const app_reset_observation_t single = {
      UINT32_C(0x20000000), APP_RESET_REASON_IWDG};
  app_reset_decoded_t decoded = app_reset_reason_decode(&single);
  CHECK(decoded.primary == APP_RESET_PRIMARY_IWDG);
  CHECK(decoded.reason_mask == APP_RESET_REASON_IWDG);
  CHECK(!decoded.multiple);

  const app_reset_observation_t multiple = {
      UINT32_C(0x34000000),
      APP_RESET_REASON_POWER_ON | APP_RESET_REASON_IWDG};
  decoded = app_reset_reason_decode(&multiple);
  CHECK(decoded.primary == APP_RESET_PRIMARY_IWDG);
  CHECK(decoded.multiple);
  CHECK((decoded.reason_mask & APP_RESET_REASON_MULTIPLE) != 0U);

  const app_reset_observation_t unknown = {UINT32_C(0x00000001), 0U};
  CHECK(app_reset_reason_decode(&unknown).primary ==
        APP_RESET_PRIMARY_UNKNOWN);

  app_reset_record_t record;
  app_reset_record_initialize(&record);
  CHECK(app_reset_record_is_valid(&record));
  for (uint32_t boot = 0U; boot < APP_RESET_LOOP_LIMIT; ++boot)
  {
    CHECK(app_reset_record_note_boot(&record, &decoded, UINT32_C(0x5301)));
  }
  CHECK(record.boot_count == APP_RESET_LOOP_LIMIT);
  CHECK(app_reset_record_loop_latched(&record));
  CHECK(app_reset_record_note_fault(&record, UINT32_C(0x5350)));
  CHECK(record.last_fault_code == UINT32_C(0x5350));
  CHECK(app_reset_record_note_stable(&record));
  CHECK(!app_reset_record_loop_latched(&record));

  record.checksum ^= UINT32_C(1);
  CHECK(!app_reset_record_is_valid(&record));
  CHECK(!app_reset_record_note_fault(&record, UINT32_C(0x5350)));
  CHECK(app_reset_record_note_boot(&record, &decoded, 0U));
  CHECK(app_reset_record_is_valid(&record));
  CHECK(record.boot_count == 1U);
  return EXIT_SUCCESS;
}

static int test_invalid_inputs_and_loop_latch(void)
{
  app_health_policy_t policy;
  app_health_input_t input = {0};
  app_health_decision_t decision;
  app_health_policy_initialize(&policy);
  CHECK(!app_health_policy_evaluate(NULL, &input, &decision));
  CHECK(!app_health_policy_evaluate(&policy, NULL, &decision));
  CHECK(!app_health_policy_evaluate(&policy, &input, NULL));
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  input.reset_loop_latched = true;
  advance_monitored_tasks(&input);
  CHECK(app_health_policy_evaluate(&policy, &input, &decision));
  CHECK(decision.state == APP_HEALTH_RESET_LOOP_LATCHED);
  CHECK(decision.feed_decision == APP_WATCHDOG_FEED_WITHHELD);
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(test_bootstrap_and_serviceable() == EXIT_SUCCESS);
  CHECK(test_stall_requires_two_epochs() == EXIT_SUCCESS);
  CHECK(test_warning_storm_does_not_force_reset() == EXIT_SUCCESS);
  CHECK(test_recovery_is_bounded() == EXIT_SUCCESS);
  CHECK(test_sensor_fault_is_locally_degraded() == EXIT_SUCCESS);
  CHECK(test_saturated_progress_counter_is_not_false_stall() == EXIT_SUCCESS);
  CHECK(test_reset_reason_and_loop_record() == EXIT_SUCCESS);
  CHECK(test_invalid_inputs_and_loop_latch() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
