#include "app_task_model.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)          \
  do                              \
  {                               \
    if (!(condition))             \
    {                             \
      return EXIT_FAILURE;        \
    }                             \
  } while (false)

static int test_contract_table(void)
{
  CHECK(app_task_model_is_valid());
  CHECK(app_task_model_count() == APP_TASK_COUNT);
  CHECK(app_task_model_contract(APP_TASK_COUNT) == NULL);

  const app_task_contract_t *protocol =
      app_task_model_contract(APP_TASK_PROTOCOL);
  const app_task_contract_t *diagnostic =
      app_task_model_contract(APP_TASK_DIAGNOSTIC);
  CHECK(protocol != NULL);
  CHECK(diagnostic != NULL);
  CHECK(strcmp(protocol->name, "protocol_task") == 0);
  CHECK(protocol->priority == 5U);
  CHECK(protocol->period_ms == 5U);
  CHECK(protocol->deadline_ms == 5U);
  CHECK(protocol->execution_budget_ms == 1U);
  CHECK(diagnostic->priority == 1U);
  CHECK(diagnostic->priority < protocol->priority);
  return EXIT_SUCCESS;
}

static int test_normal_absolute_releases(void)
{
  const app_task_contract_t *contract =
      app_task_model_contract(APP_TASK_PROTOCOL);
  app_task_runtime_t runtime;
  app_task_runtime_initialize(&runtime, 100U);

  CHECK(app_task_runtime_record_cycle(&runtime, contract, 100U, 101U));
  CHECK(runtime.next_release_tick == 105U);
  CHECK(runtime.release_count == 1U);
  CHECK(runtime.missed_release_count == 0U);
  CHECK(runtime.deadline_miss_count == 0U);
  CHECK(runtime.budget_overrun_count == 0U);

  CHECK(app_task_runtime_record_cycle(&runtime, contract, 105U, 106U));
  CHECK(runtime.next_release_tick == 110U);
  CHECK(runtime.release_count == 2U);
  return EXIT_SUCCESS;
}

static int test_late_wake_skips_history(void)
{
  const app_task_contract_t *contract =
      app_task_model_contract(APP_TASK_PROTOCOL);
  app_task_runtime_t runtime;
  app_task_runtime_initialize(&runtime, 100U);

  CHECK(app_task_runtime_record_cycle(&runtime, contract, 112U, 113U));
  CHECK(runtime.release_count == 1U);
  CHECK(runtime.missed_release_count == 2U);
  CHECK(runtime.deadline_miss_count == 1U);
  CHECK(runtime.budget_overrun_count == 0U);
  CHECK(runtime.next_release_tick == 115U);
  return EXIT_SUCCESS;
}

static int test_single_and_multi_period_overload(void)
{
  const app_task_contract_t *contract =
      app_task_model_contract(APP_TASK_PROTOCOL);
  app_task_runtime_t single;
  app_task_runtime_initialize(&single, 0U);
  CHECK(app_task_runtime_record_cycle(&single, contract, 0U, 6U));
  CHECK(single.missed_release_count == 1U);
  CHECK(single.deadline_miss_count == 1U);
  CHECK(single.budget_overrun_count == 1U);
  CHECK(single.next_release_tick == 10U);

  app_task_runtime_t multiple;
  app_task_runtime_initialize(&multiple, 0U);
  CHECK(app_task_runtime_record_cycle(&multiple, contract, 0U, 17U));
  CHECK(multiple.missed_release_count == 3U);
  CHECK(multiple.deadline_miss_count == 1U);
  CHECK(multiple.budget_overrun_count == 1U);
  CHECK(multiple.next_release_tick == 20U);
  return EXIT_SUCCESS;
}

static int test_exact_deadline_keeps_next_release(void)
{
  const app_task_contract_t *contract =
      app_task_model_contract(APP_TASK_PROTOCOL);
  app_task_runtime_t runtime;
  app_task_runtime_initialize(&runtime, 100U);

  CHECK(app_task_runtime_record_cycle(&runtime, contract, 100U, 105U));
  CHECK(runtime.next_release_tick == 105U);
  CHECK(runtime.missed_release_count == 0U);
  CHECK(runtime.deadline_miss_count == 0U);
  CHECK(runtime.budget_overrun_count == 1U);
  return EXIT_SUCCESS;
}

static int test_tick_wrap(void)
{
  const app_task_contract_t *contract =
      app_task_model_contract(APP_TASK_PROTOCOL);
  app_task_runtime_t runtime;
  const uint32_t first_release = UINT32_MAX - 2U;
  app_task_runtime_initialize(&runtime, first_release);

  CHECK(app_task_runtime_record_cycle(
      &runtime, contract, first_release, first_release + 1U));
  CHECK(runtime.next_release_tick == 2U);
  CHECK(runtime.missed_release_count == 0U);
  CHECK(app_task_runtime_record_cycle(&runtime, contract, 2U, 3U));
  CHECK(runtime.next_release_tick == 7U);
  return EXIT_SUCCESS;
}

static int test_task_state_is_independent(void)
{
  const app_task_contract_t *protocol =
      app_task_model_contract(APP_TASK_PROTOCOL);
  const app_task_contract_t *diagnostic =
      app_task_model_contract(APP_TASK_DIAGNOSTIC);
  app_task_runtime_t protocol_runtime;
  app_task_runtime_t diagnostic_runtime;
  app_task_runtime_initialize(&protocol_runtime, 0U);
  app_task_runtime_initialize(&diagnostic_runtime, 0U);

  CHECK(app_task_runtime_record_cycle(
      &diagnostic_runtime, diagnostic, 0U, 3U));
  CHECK(diagnostic_runtime.budget_overrun_count == 1U);
  CHECK(protocol_runtime.release_count == 0U);
  CHECK(protocol_runtime.missed_release_count == 0U);
  CHECK(protocol_runtime.deadline_miss_count == 0U);
  CHECK(protocol_runtime.budget_overrun_count == 0U);
  CHECK(protocol->priority > diagnostic->priority);
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(test_contract_table() == EXIT_SUCCESS);
  CHECK(test_normal_absolute_releases() == EXIT_SUCCESS);
  CHECK(test_late_wake_skips_history() == EXIT_SUCCESS);
  CHECK(test_single_and_multi_period_overload() == EXIT_SUCCESS);
  CHECK(test_exact_deadline_keeps_next_release() == EXIT_SUCCESS);
  CHECK(test_tick_wrap() == EXIT_SUCCESS);
  CHECK(test_task_state_is_independent() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
