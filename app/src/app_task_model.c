#include "app_task_model.h"

#include <limits.h>

#define APP_TASK_MAX_PRIORITIES (6U)

static const app_task_contract_t app_task_contracts[APP_TASK_COUNT] = {
    {APP_TASK_PROTOCOL, "protocol_task", 5U, 5U, 5U, 1U},
    {APP_TASK_ACQUISITION, "acquisition_task", 4U, 20U, 20U, 2U},
    {APP_TASK_CAN, "can_task", 3U, 100U, 100U, 2U},
    {APP_TASK_HEALTH, "health_task", 2U, 1000U, 1000U, 5U},
    {APP_TASK_DIAGNOSTIC, "diagnostic_task", 1U, 200U, 200U, 2U},
};

_Static_assert((sizeof(app_task_contracts) / sizeof(app_task_contracts[0])) ==
                   APP_TASK_COUNT,
               "task contract table must cover every task");

static uint32_t app_task_saturating_add(uint32_t value, uint32_t increment)
{
  if (increment > (UINT32_MAX - value))
  {
    return UINT32_MAX;
  }
  return value + increment;
}

static bool app_task_tick_reached(uint32_t now, uint32_t target)
{
  return (now - target) < UINT32_C(0x80000000);
}

size_t app_task_model_count(void)
{
  return APP_TASK_COUNT;
}

const app_task_contract_t *app_task_model_contract(app_task_id_t id)
{
  if ((unsigned int)id >= (unsigned int)APP_TASK_COUNT)
  {
    return NULL;
  }
  return &app_task_contracts[id];
}

bool app_task_model_is_valid(void)
{
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    const app_task_contract_t *contract = &app_task_contracts[index];
    if (((size_t)contract->id != index) || (contract->name == NULL) ||
        (contract->priority == 0U) ||
        (contract->priority >= APP_TASK_MAX_PRIORITIES) ||
        (contract->period_ms == 0U) ||
        (contract->deadline_ms == 0U) ||
        (contract->deadline_ms > contract->period_ms) ||
        (contract->execution_budget_ms == 0U) ||
        (contract->execution_budget_ms >= contract->deadline_ms))
    {
      return false;
    }

    if ((index > 0U) &&
        (app_task_contracts[index - 1U].priority <= contract->priority))
    {
      return false;
    }
  }
  return true;
}

void app_task_runtime_initialize(app_task_runtime_t *runtime,
                                 uint32_t first_release_tick)
{
  if (runtime == NULL)
  {
    return;
  }

  runtime->next_release_tick = first_release_tick;
  runtime->release_count = 0U;
  runtime->missed_release_count = 0U;
  runtime->deadline_miss_count = 0U;
  runtime->budget_overrun_count = 0U;
}

bool app_task_runtime_record_cycle(app_task_runtime_t *runtime,
                                   const app_task_contract_t *contract,
                                   uint32_t actual_start_tick,
                                   uint32_t actual_finish_tick)
{
  if ((runtime == NULL) || (contract == NULL) ||
      (contract->period_ms == 0U) ||
      !app_task_tick_reached(actual_start_tick, runtime->next_release_tick) ||
      !app_task_tick_reached(actual_finish_tick, actual_start_tick))
  {
    return false;
  }

  const uint32_t release_tick = runtime->next_release_tick;
  const uint32_t elapsed_to_finish = actual_finish_tick - release_tick;
  const uint32_t execution_ticks = actual_finish_tick - actual_start_tick;
  const uint32_t crossed_release_count =
      (elapsed_to_finish == 0U)
          ? 0U
          : ((elapsed_to_finish - 1U) / contract->period_ms);

  runtime->release_count =
      app_task_saturating_add(runtime->release_count, 1U);
  runtime->missed_release_count = app_task_saturating_add(
      runtime->missed_release_count, crossed_release_count);
  if (elapsed_to_finish > contract->deadline_ms)
  {
    runtime->deadline_miss_count =
        app_task_saturating_add(runtime->deadline_miss_count, 1U);
  }
  if (execution_ticks > contract->execution_budget_ms)
  {
    runtime->budget_overrun_count =
        app_task_saturating_add(runtime->budget_overrun_count, 1U);
  }

  runtime->next_release_tick =
      release_tick + ((crossed_release_count + 1U) * contract->period_ms);
  return true;
}

bool app_task_runtime_release_due(const app_task_runtime_t *runtime,
                                  uint32_t now_tick)
{
  return (runtime != NULL) &&
         app_task_tick_reached(now_tick, runtime->next_release_tick);
}

uint32_t app_task_runtime_ticks_until_release(
    const app_task_runtime_t *runtime,
    uint32_t now_tick)
{
  if ((runtime == NULL) || app_task_runtime_release_due(runtime, now_tick))
  {
    return 0U;
  }
  return runtime->next_release_tick - now_tick;
}
