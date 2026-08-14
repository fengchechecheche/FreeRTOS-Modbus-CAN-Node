#include "app_rtos.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "app_boot.h"
#include "app_resource_budget.h"
#include "app_rs485_smoke.h"
#include "app_task_model.h"
#include "app_transport_policy.h"
#include "bsp_rs485.h"
#include "queue.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include "task.h"

#ifndef P5_IRQ_NOTIFICATION_SMOKE_ENABLE
#define P5_IRQ_NOTIFICATION_SMOKE_ENABLE (0)
#endif

_Static_assert(sizeof(StackType_t) == APP_RESOURCE_STACK_WORD_BYTES,
               "resource budget assumes 32-bit FreeRTOS stack words");
_Static_assert(APP_RESOURCE_PROTOCOL_STACK_WORDS >= configMINIMAL_STACK_SIZE,
               "protocol stack is below the FreeRTOS minimum");
_Static_assert(APP_RESOURCE_ACQUISITION_STACK_WORDS >= configMINIMAL_STACK_SIZE,
               "acquisition stack is below the FreeRTOS minimum");
_Static_assert(APP_RESOURCE_CAN_STACK_WORDS >= configMINIMAL_STACK_SIZE,
               "CAN stack is below the FreeRTOS minimum");
_Static_assert(APP_RESOURCE_HEALTH_STACK_WORDS >= configMINIMAL_STACK_SIZE,
               "health stack is below the FreeRTOS minimum");
_Static_assert(APP_RESOURCE_DIAGNOSTIC_STACK_WORDS >= configMINIMAL_STACK_SIZE,
               "diagnostic stack is below the FreeRTOS minimum");
_Static_assert(sizeof(app_transport_event_t) ==
                   APP_RESOURCE_DIAGNOSTIC_EVENT_ITEM_BYTES,
               "diagnostic event resource item size drifted");
_Static_assert(APP_TRANSPORT_EVENT_QUEUE_DEPTH ==
                   APP_RESOURCE_DIAGNOSTIC_EVENT_QUEUE_DEPTH,
               "diagnostic event queue depth drifted");
_Static_assert(APP_TRANSPORT_EVENT_DRAIN_BUDGET ==
                   APP_RESOURCE_DIAGNOSTIC_EVENT_DRAIN_BUDGET,
               "diagnostic event drain budget drifted");

typedef void (*app_rtos_service_t)(void);

static StaticTask_t app_rtos_task_controls[APP_TASK_COUNT];
static StackType_t
    app_rtos_protocol_stack[APP_RESOURCE_PROTOCOL_STACK_WORDS];
static StackType_t
    app_rtos_acquisition_stack[APP_RESOURCE_ACQUISITION_STACK_WORDS];
static StackType_t app_rtos_can_stack[APP_RESOURCE_CAN_STACK_WORDS];
static StackType_t app_rtos_health_stack[APP_RESOURCE_HEALTH_STACK_WORDS];
static StackType_t
    app_rtos_diagnostic_stack[APP_RESOURCE_DIAGNOSTIC_STACK_WORDS];
static StackType_t *const app_rtos_task_stacks[APP_TASK_COUNT] = {
    app_rtos_protocol_stack,
    app_rtos_acquisition_stack,
    app_rtos_can_stack,
    app_rtos_health_stack,
    app_rtos_diagnostic_stack,
};
static const uint32_t app_rtos_task_stack_words[APP_TASK_COUNT] = {
    APP_RESOURCE_PROTOCOL_STACK_WORDS,
    APP_RESOURCE_ACQUISITION_STACK_WORDS,
    APP_RESOURCE_CAN_STACK_WORDS,
    APP_RESOURCE_HEALTH_STACK_WORDS,
    APP_RESOURCE_DIAGNOSTIC_STACK_WORDS,
};
static TaskHandle_t app_rtos_task_handles[APP_TASK_COUNT];
static app_task_runtime_t app_rtos_task_runtime[APP_TASK_COUNT];
static app_rtos_health_snapshot_t app_rtos_health_snapshot;
static app_rtos_resource_snapshot_t app_rtos_resource_snapshot;
static volatile uint32_t app_rtos_current_fault_code = APP_RTOS_FAULT_NONE;
static bsp_rs485_irq_latency_summary_t app_rtos_irq_latency_summary;
static StaticQueue_t app_rtos_event_queue_control;
static uint8_t app_rtos_event_queue_storage[
    APP_RESOURCE_DIAGNOSTIC_EVENT_STORAGE_BYTES];
static QueueHandle_t app_rtos_event_queue;
static StaticSemaphore_t app_rtos_snapshot_mutex_control;
static SemaphoreHandle_t app_rtos_snapshot_mutex;
static app_transport_counters_t app_rtos_transport_counters;
#if P5_IRQ_NOTIFICATION_SMOKE_ENABLE
static volatile uint32_t app_rtos_irq_first_cycles;
static volatile bool app_rtos_irq_first_cycles_valid;
#endif

static uint32_t app_rtos_saturating_add(uint32_t value, uint32_t increment)
{
  if (increment > (UINT32_MAX - value))
  {
    return UINT32_MAX;
  }
  return value + increment;
}

static void app_rtos_note_snapshot_contention(bool writer)
{
  taskENTER_CRITICAL();
  app_transport_note_snapshot_contention(&app_rtos_transport_counters, writer);
  taskEXIT_CRITICAL();
}

static bool app_rtos_snapshot_take(bool writer)
{
  if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
  {
    return true;
  }

  if ((app_rtos_snapshot_mutex == NULL) ||
      (xSemaphoreTake(app_rtos_snapshot_mutex, 0U) != pdTRUE))
  {
    app_rtos_note_snapshot_contention(writer);
    return false;
  }
  return true;
}

static void app_rtos_snapshot_give(void)
{
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
    (void)xSemaphoreGive(app_rtos_snapshot_mutex);
  }
}

static void app_rtos_noop_service(void)
{
}

static void app_rtos_protocol_service(void)
{
  (void)bsp_rs485_poll();
  app_rs485_smoke_poll();
}

static void app_rtos_protocol_event_service(void)
{
  if (bsp_rs485_service_irq_events() != 0U)
  {
    app_rs485_smoke_poll();
  }
}

static void app_rtos_rs485_notify_from_isr(uint32_t event_mask)
{
  TaskHandle_t protocol_handle = app_rtos_task_handles[APP_TASK_PROTOCOL];
  if (protocol_handle == NULL)
  {
    return;
  }

#if P5_IRQ_NOTIFICATION_SMOKE_ENABLE
  if (!app_rtos_irq_first_cycles_valid)
  {
    app_rtos_irq_first_cycles = DWT->CYCCNT;
    app_rtos_irq_first_cycles_valid = true;
  }
#endif

  BaseType_t higher_priority_task_woken = pdFALSE;
  (void)xTaskNotifyFromISR(protocol_handle,
                           event_mask,
                           eSetBits,
                           &higher_priority_task_woken);
  portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void app_rtos_record_irq_latency(void)
{
#if P5_IRQ_NOTIFICATION_SMOKE_ENABLE
  uint32_t isr_cycles = 0U;
  uint32_t task_cycles = 0U;
  bool available = false;

  taskENTER_CRITICAL();
  if (app_rtos_irq_first_cycles_valid)
  {
    isr_cycles = app_rtos_irq_first_cycles;
    task_cycles = DWT->CYCCNT;
    app_rtos_irq_first_cycles_valid = false;
    available = true;
  }
  taskEXIT_CRITICAL();

  if (available)
  {
    if (app_rtos_snapshot_take(true))
    {
      bsp_rs485_irq_latency_record(&app_rtos_irq_latency_summary,
                                   isr_cycles,
                                   task_cycles);
      app_rtos_snapshot_give();
    }
  }
#endif
}

static void app_rtos_update_resource_snapshot(void)
{
  app_rtos_resource_snapshot_t snapshot;
  const bool scheduler_running =
      xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;

  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    snapshot.task[index].configured_words = app_rtos_task_stack_words[index];
    snapshot.task[index].minimum_free_words = 0U;
    snapshot.task[index].measured = false;

    if (scheduler_running && (app_rtos_task_handles[index] != NULL))
    {
      snapshot.task[index].minimum_free_words =
          (uint32_t)uxTaskGetStackHighWaterMark(app_rtos_task_handles[index]);
      snapshot.task[index].measured = true;
    }
  }

  if (!scheduler_running)
  {
    app_rtos_resource_snapshot = snapshot;
  }
  else if (app_rtos_snapshot_take(true))
  {
    app_rtos_resource_snapshot = snapshot;
    app_rtos_snapshot_give();
  }
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
  taskEXIT_CRITICAL();

  if (app_rtos_snapshot_take(true))
  {
    app_rtos_health_snapshot = snapshot;
    app_rtos_snapshot_give();
  }

  app_rtos_update_resource_snapshot();
}

static void app_rtos_diagnostic_service(void)
{
  size_t drained_count = 0U;
  app_transport_event_t event;
  while ((drained_count < APP_TRANSPORT_EVENT_DRAIN_BUDGET) &&
         (xQueueReceive(app_rtos_event_queue, &event, 0U) == pdTRUE))
  {
    ++drained_count;
  }

  taskENTER_CRITICAL();
  app_transport_note_events_drained(&app_rtos_transport_counters,
                                    drained_count);
  taskEXIT_CRITICAL();

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

static _Noreturn void app_rtos_run_protocol(void)
{
  const app_task_contract_t *contract =
      app_task_model_contract(APP_TASK_PROTOCOL);
  if (contract == NULL)
  {
    app_rtos_fail_stop(APP_RTOS_FAULT_MODEL);
  }

  app_task_runtime_t *runtime = &app_rtos_task_runtime[APP_TASK_PROTOCOL];
  app_task_runtime_initialize(runtime, (uint32_t)xTaskGetTickCount());

  for (;;)
  {
    app_rtos_protocol_event_service();

    TickType_t now_tick = xTaskGetTickCount();
    if (app_task_runtime_release_due(runtime, (uint32_t)now_tick))
    {
      const TickType_t actual_start_tick = now_tick;
      app_rtos_protocol_service();
      const TickType_t actual_finish_tick = xTaskGetTickCount();
      if (!app_task_runtime_record_cycle(runtime,
                                         contract,
                                         (uint32_t)actual_start_tick,
                                         (uint32_t)actual_finish_tick))
      {
        app_rtos_fail_stop(APP_RTOS_FAULT_CYCLE);
      }
      continue;
    }

    const TickType_t wait_ticks = (TickType_t)
        app_task_runtime_ticks_until_release(runtime, (uint32_t)now_tick);
    uint32_t notification_value = 0U;
    if (xTaskNotifyWait(0U,
                        UINT32_MAX,
                        &notification_value,
                        wait_ticks) == pdTRUE)
    {
      (void)notification_value;
      app_rtos_record_irq_latency();
    }
  }
}

static void app_rtos_protocol_task(void *context)
{
  (void)context;
  app_rtos_run_protocol();
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

  app_transport_counters_initialize(&app_rtos_transport_counters);
  app_rtos_event_queue = xQueueCreateStatic(
      APP_TRANSPORT_EVENT_QUEUE_DEPTH,
      sizeof(app_transport_event_t),
      app_rtos_event_queue_storage,
      &app_rtos_event_queue_control);
  if (app_rtos_event_queue == NULL)
  {
    app_rtos_current_fault_code = APP_RTOS_FAULT_QUEUE_CREATE;
    return APP_RTOS_ERROR;
  }

  app_rtos_snapshot_mutex =
      xSemaphoreCreateMutexStatic(&app_rtos_snapshot_mutex_control);
  if (app_rtos_snapshot_mutex == NULL)
  {
    app_rtos_current_fault_code = APP_RTOS_FAULT_MUTEX_CREATE;
    return APP_RTOS_ERROR;
  }

  app_rtos_update_resource_snapshot();
  bsp_rs485_irq_latency_reset(&app_rtos_irq_latency_summary);
#if P5_IRQ_NOTIFICATION_SMOKE_ENABLE
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  app_rtos_irq_first_cycles = 0U;
  app_rtos_irq_first_cycles_valid = false;
#endif

  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    const app_task_contract_t *contract =
        app_task_model_contract((app_task_id_t)index);
    if (contract == NULL)
    {
      app_rtos_current_fault_code = APP_RTOS_FAULT_TASK_CREATE;
      return APP_RTOS_ERROR;
    }

    app_rtos_task_handles[index] = xTaskCreateStatic(
        task_entries[index], contract->name, app_rtos_task_stack_words[index],
        NULL, (UBaseType_t)contract->priority, app_rtos_task_stacks[index],
        &app_rtos_task_controls[index]);
    if (app_rtos_task_handles[index] == NULL)
    {
      app_rtos_current_fault_code = APP_RTOS_FAULT_TASK_CREATE;
      return APP_RTOS_ERROR;
    }
  }

  bsp_rs485_register_irq_notifier(app_rtos_rs485_notify_from_isr);

  return APP_RTOS_OK;
}

bool app_rtos_publish_diagnostic_event(const app_transport_event_t *event)
{
  BaseType_t queued = pdFALSE;
  if ((app_rtos_event_queue != NULL) && app_transport_event_is_valid(event))
  {
    queued = xQueueSend(app_rtos_event_queue, event, 0U);
  }

  taskENTER_CRITICAL();
  const app_transport_event_result_t result = app_transport_event_admit(
      &app_rtos_transport_counters, event, queued == pdTRUE);
  taskEXIT_CRITICAL();
  return result == APP_TRANSPORT_EVENT_ACCEPTED;
}

bool app_rtos_get_health_snapshot(app_rtos_health_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return false;
  }

  if (!app_rtos_snapshot_take(false))
  {
    return false;
  }
  *snapshot = app_rtos_health_snapshot;
  app_rtos_snapshot_give();
  return true;
}

bool app_rtos_get_resource_snapshot(app_rtos_resource_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return false;
  }

  if (!app_rtos_snapshot_take(false))
  {
    return false;
  }
  *snapshot = app_rtos_resource_snapshot;
  app_rtos_snapshot_give();
  return true;
}

bool app_rtos_get_irq_latency_snapshot(
    app_rtos_irq_latency_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return false;
  }

  if (!app_rtos_snapshot_take(false))
  {
    return false;
  }
  snapshot->sample_count = app_rtos_irq_latency_summary.sample_count;
  snapshot->minimum_cycles = app_rtos_irq_latency_summary.minimum_cycles;
  snapshot->maximum_cycles = app_rtos_irq_latency_summary.maximum_cycles;
  snapshot->last_cycles = app_rtos_irq_latency_summary.last_cycles;
  snapshot->measured = app_rtos_irq_latency_summary.measured;
  snapshot->enabled = P5_IRQ_NOTIFICATION_SMOKE_ENABLE != 0;
  app_rtos_snapshot_give();
  return true;
}

bool app_rtos_get_transport_counters(app_transport_counters_t *counters)
{
  if (counters == NULL)
  {
    return false;
  }

  taskENTER_CRITICAL();
  *counters = app_rtos_transport_counters;
  taskEXIT_CRITICAL();
  return true;
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
