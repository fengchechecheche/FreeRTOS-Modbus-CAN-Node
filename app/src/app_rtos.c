#include "app_rtos.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "app_boot.h"
#include "app_rs485_smoke.h"
#include "app_task_model.h"
#include "bsp_rs485.h"
#include "stm32f4xx.h"
#include "task.h"

#define APP_RTOS_PROVISIONAL_STACK_WORDS (256U)

typedef void (*app_rtos_service_t)(void);

static StaticTask_t app_rtos_task_controls[APP_TASK_COUNT];
static StackType_t
    app_rtos_task_stacks[APP_TASK_COUNT][APP_RTOS_PROVISIONAL_STACK_WORDS];
static app_task_runtime_t app_rtos_task_runtime[APP_TASK_COUNT];
static volatile app_rtos_health_snapshot_t app_rtos_health_snapshot;
static volatile uint32_t app_rtos_current_fault_code = APP_RTOS_FAULT_NONE;

static uint32_t app_rtos_saturating_add(uint32_t value, uint32_t increment)
{
  if (increment > (UINT32_MAX - value))
  {
    return UINT32_MAX;
  }
  return value + increment;
}

static void app_rtos_noop_service(void)
{
}

static void app_rtos_protocol_service(void)
{
  (void)bsp_rs485_poll();
  app_rs485_smoke_poll();
}

static void app_rtos_health_service(void)
{
  app_rtos_health_snapshot_t snapshot = {0U, 0U, 0U, 0U};

  taskENTER_CRITICAL();
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    snapshot.release_count = app_rtos_saturating_add(
        snapshot.release_count, app_rtos_task_runtime[index].release_count);
    snapshot.missed_release_count = app_rtos_saturating_add(
        snapshot.missed_release_count,
        app_rtos_task_runtime[index].missed_release_count);
    snapshot.deadline_miss_count = app_rtos_saturating_add(
        snapshot.deadline_miss_count,
        app_rtos_task_runtime[index].deadline_miss_count);
    snapshot.budget_overrun_count = app_rtos_saturating_add(
        snapshot.budget_overrun_count,
        app_rtos_task_runtime[index].budget_overrun_count);
  }
  app_rtos_health_snapshot = snapshot;
  taskEXIT_CRITICAL();
}

static void app_rtos_diagnostic_service(void)
{
  app_boot_diagnostic_service();
}

static _Noreturn void app_rtos_run_periodic(app_task_id_t id,
                                             app_rtos_service_t service)
{
  const app_task_contract_t *contract = app_task_model_contract(id);
  if ((contract == NULL) || (service == NULL))
  {
    app_rtos_fail_stop(APP_RTOS_FAULT_MODEL);
  }

  const TickType_t period_ticks = pdMS_TO_TICKS(contract->period_ms);
  if (period_ticks == 0U)
  {
    app_rtos_fail_stop(APP_RTOS_FAULT_MODEL);
  }

  app_task_runtime_t *runtime = &app_rtos_task_runtime[id];
  app_task_runtime_initialize(runtime, (uint32_t)xTaskGetTickCount());

  for (;;)
  {
    const TickType_t actual_start_tick = xTaskGetTickCount();
    service();
    const TickType_t actual_finish_tick = xTaskGetTickCount();

    if (!app_task_runtime_record_cycle(runtime,
                                       contract,
                                       (uint32_t)actual_start_tick,
                                       (uint32_t)actual_finish_tick))
    {
      app_rtos_fail_stop(APP_RTOS_FAULT_CYCLE);
    }

    TickType_t previous_wake_tick =
        (TickType_t)(runtime->next_release_tick - period_ticks);
    vTaskDelayUntil(&previous_wake_tick, period_ticks);
  }
}

static void app_rtos_protocol_task(void *context)
{
  (void)context;
  app_rtos_run_periodic(APP_TASK_PROTOCOL, app_rtos_protocol_service);
}

static void app_rtos_acquisition_task(void *context)
{
  (void)context;
  app_rtos_run_periodic(APP_TASK_ACQUISITION, app_rtos_noop_service);
}

static void app_rtos_can_task(void *context)
{
  (void)context;
  app_rtos_run_periodic(APP_TASK_CAN, app_rtos_noop_service);
}

static void app_rtos_health_task(void *context)
{
  (void)context;
  app_rtos_run_periodic(APP_TASK_HEALTH, app_rtos_health_service);
}

static void app_rtos_diagnostic_task(void *context)
{
  (void)context;
  app_rtos_run_periodic(APP_TASK_DIAGNOSTIC, app_rtos_diagnostic_service);
}

app_rtos_status_t app_rtos_initialize(void)
{
  static TaskFunction_t const task_entries[APP_TASK_COUNT] = {
      app_rtos_protocol_task,
      app_rtos_acquisition_task,
      app_rtos_can_task,
      app_rtos_health_task,
      app_rtos_diagnostic_task,
  };

  if (!app_task_model_is_valid() || (sizeof(TickType_t) != sizeof(uint32_t)))
  {
    return APP_RTOS_ERROR;
  }

  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    const app_task_contract_t *contract =
        app_task_model_contract((app_task_id_t)index);
    if ((contract == NULL) ||
        (xTaskCreateStatic(task_entries[index],
                           contract->name,
                           APP_RTOS_PROVISIONAL_STACK_WORDS,
                           NULL,
                           (UBaseType_t)contract->priority,
                           app_rtos_task_stacks[index],
                           &app_rtos_task_controls[index]) == NULL))
    {
      app_rtos_current_fault_code = APP_RTOS_FAULT_TASK_CREATE;
      return APP_RTOS_ERROR;
    }
  }

  return APP_RTOS_OK;
}

void app_rtos_get_health_snapshot(app_rtos_health_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return;
  }

  taskENTER_CRITICAL();
  *snapshot = app_rtos_health_snapshot;
  taskEXIT_CRITICAL();
}

uint32_t app_rtos_fault_code(void)
{
  return app_rtos_current_fault_code;
}

_Noreturn void app_rtos_fail_stop(uint32_t fault_code)
{
  app_rtos_current_fault_code = fault_code;
  __disable_irq();
  __DSB();
  __ISB();
  for (;;)
  {
    __WFI();
  }
}
