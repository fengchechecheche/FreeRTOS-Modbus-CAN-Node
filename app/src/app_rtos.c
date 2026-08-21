#include "app_rtos.h"

#ifndef P5_ADXL345_HIL_DIAGNOSTIC_ENABLE
#define P5_ADXL345_HIL_DIAGNOSTIC_ENABLE (0)
#endif

#ifndef P5_CAN_ACK_RX_DIAGNOSTIC_ENABLE
#define P5_CAN_ACK_RX_DIAGNOSTIC_ENABLE (0)
#endif

#ifndef P5_CAN_ACK_TX_DIAGNOSTIC_ENABLE
#define P5_CAN_ACK_TX_DIAGNOSTIC_ENABLE (0)
#endif

#ifndef P5_CAN_BOUNDED_ECHO_DIAGNOSTIC_ENABLE
#define P5_CAN_BOUNDED_ECHO_DIAGNOSTIC_ENABLE (0)
#endif

#ifndef P5_SOAK_DIAGNOSTIC_ENABLE
#define P5_SOAK_DIAGNOSTIC_ENABLE (0)
#endif

#define APP_CAN_ACK_DIAGNOSTIC_ENABLE                                      \
  (P5_CAN_ACK_RX_DIAGNOSTIC_ENABLE || P5_CAN_ACK_TX_DIAGNOSTIC_ENABLE)
#define APP_CAN_TX_SUPPRESSED_DIAGNOSTIC_ENABLE                            \
  (APP_CAN_ACK_DIAGNOSTIC_ENABLE || P5_CAN_BOUNDED_ECHO_DIAGNOSTIC_ENABLE)

#include <stddef.h>
#if P5_ADXL345_HIL_DIAGNOSTIC_ENABLE || APP_CAN_ACK_DIAGNOSTIC_ENABLE
#include <stdio.h>
#endif

#include "FreeRTOS.h"
#include "app_adxl345.h"
#include "app_bme280.h"
#include "app_boot.h"
#if APP_CAN_ACK_DIAGNOSTIC_ENABLE
#include "app_can_ack_diagnostic.h"
#endif
#include "app_can_runtime.h"
#include "app_health_policy.h"
#include "app_measurement.h"
#include "app_modbus_transport.h"
#include "app_reset_reason.h"
#include "app_resource_budget.h"
#include "app_rs485_smoke.h"
#include "app_sensor_monitor.h"
#if P5_SOAK_DIAGNOSTIC_ENABLE
#include "app_soak_diagnostic.h"
#endif
#include "app_task_model.h"
#include "app_transport_policy.h"
#include "app_veml7700.h"
#include "bsp_adxl345_irq.h"
#include "bsp_can.h"
#include "bsp_clock.h"
#include "bsp_rs485.h"
#include "bsp_watchdog.h"
#include "queue.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "task.h"
#if P5_ADXL345_HIL_DIAGNOSTIC_ENABLE || APP_CAN_ACK_DIAGNOSTIC_ENABLE
#include "main.h"
#endif
#include "usart.h"

#ifndef P5_IRQ_NOTIFICATION_SMOKE_ENABLE
#define P5_IRQ_NOTIFICATION_SMOKE_ENABLE (0)
#endif

#ifndef P5_RS485_LOOPBACK_SMOKE_ENABLE
#define P5_RS485_LOOPBACK_SMOKE_ENABLE (0)
#endif

#ifndef P5_IWDG_RESET_SMOKE_ENABLE
#define P5_IWDG_RESET_SMOKE_ENABLE (0)
#endif

#define APP_CAN_TELEMETRY_PERIOD_MS UINT32_C(1000)
#define APP_CAN_RX_DRAIN_BUDGET UINT32_C(2)
#define APP_CAN_EVENT_SOURCE UINT8_C(5)
#define APP_CAN_EVENT_START_FAILED UINT16_C(0x0301)
#define APP_CAN_EVENT_BUS_OFF UINT16_C(0x0302)
#define APP_CAN_EVENT_RECOVERED UINT16_C(0x0303)
#define APP_CAN_DIAGNOSTIC_REPORT_CAPACITY (112U)
#define APP_CAN_DIAGNOSTIC_UART_TIMEOUT_MS UINT32_C(100)
#define APP_IWDG_SMOKE_PRIME_FEEDS UINT32_C(3)
#define APP_IWDG_STABLE_FEEDS UINT32_C(5)
#if APP_CAN_ACK_DIAGNOSTIC_ENABLE
#define APP_CAN_ACK_RX_TIMEOUT_MS UINT32_C(3000)
#define APP_CAN_ACK_TX_DELAY_MS UINT32_C(2000)
#define APP_CAN_ACK_TX_TIMEOUT_MS UINT32_C(1000)
#define APP_CAN_ACK_RX_REPORT_DELAY_MS UINT32_C(250)
#define APP_CAN_ACK_REPORT_CAPACITY (384U)
#define APP_CAN_ACK_UART_TIMEOUT_MS UINT32_C(100)
#endif
#if P5_ADXL345_HIL_DIAGNOSTIC_ENABLE
#define APP_ADXL345_HIL_REPORT_CAPACITY (448U)
#define APP_ADXL345_HIL_REPORT_INTERVAL_MS UINT32_C(1000)
#define APP_ADXL345_HIL_REPORT_LIMIT UINT32_C(180)
#define APP_ADXL345_HIL_UART_TIMEOUT_MS UINT32_C(100)
#endif
#if P5_SOAK_DIAGNOSTIC_ENABLE
#define APP_SOAK_DIAGNOSTIC_INTERVAL_MS UINT32_C(60000)
#define APP_SOAK_DIAGNOSTIC_UART_TIMEOUT_MS UINT32_C(100)
#define APP_SOAK_CAN_CAPACITY                                              \
  (APP_CAN_EVENT_FIFO_DEPTH + APP_CAN_TELEMETRY_GROUP_COUNT + UINT32_C(1))
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
static app_measurement_model_t app_rtos_measurement_model;
static app_measurement_inputs_t app_rtos_measurement_inputs;
static app_measurement_snapshot_t app_rtos_measurement_snapshot;
static app_sensor_monitor_t app_rtos_sensor_monitor;
static app_sensor_monitor_snapshot_t app_rtos_sensor_monitor_snapshot;
#if P5_SOAK_DIAGNOSTIC_ENABLE
static uint32_t app_rtos_soak_diagnostic_last_report_ms;
static bool app_rtos_soak_diagnostic_has_report;
static char app_rtos_soak_diagnostic_report[
    APP_SOAK_DIAGNOSTIC_LINE_CAPACITY];
static app_rtos_health_snapshot_t app_rtos_soak_health;
static app_rtos_resource_snapshot_t app_rtos_soak_resource;
static app_rtos_transport_snapshot_t app_rtos_soak_transport;
static app_can_runtime_snapshot_t app_rtos_soak_can;
static app_measurement_snapshot_t app_rtos_soak_measurement;
static app_sensor_monitor_snapshot_t app_rtos_soak_monitor;
static app_reset_decoded_t app_rtos_soak_reset;
static app_reset_record_t app_rtos_soak_record;
static app_modbus_transport_diagnostics_t app_rtos_soak_modbus;
static app_soak_diagnostic_snapshot_t app_rtos_soak_snapshot;
#endif
#if P5_ADXL345_HIL_DIAGNOSTIC_ENABLE
static app_adxl345_snapshot_t app_rtos_adxl345_snapshot;
static uint32_t app_rtos_adxl345_hil_last_report_ms;
static uint32_t app_rtos_adxl345_hil_report_count;
static char app_rtos_adxl345_hil_report[APP_ADXL345_HIL_REPORT_CAPACITY];
static app_adxl345_register_diagnostic_t
    app_rtos_adxl345_register_owner;
static app_adxl345_register_diagnostic_t
    app_rtos_adxl345_register_snapshot;
static app_adxl345_polling_diagnostic_t
    app_rtos_adxl345_polling_owner;
static app_adxl345_polling_diagnostic_t
    app_rtos_adxl345_polling_snapshot;
#endif
static uint32_t app_rtos_modbus_image_generation;
static volatile uint32_t app_rtos_current_fault_code = APP_RTOS_FAULT_NONE;
static bsp_rs485_irq_latency_summary_t app_rtos_irq_latency_summary;
static StaticQueue_t app_rtos_event_queue_control;
static uint8_t app_rtos_event_queue_storage[
    APP_RESOURCE_DIAGNOSTIC_EVENT_STORAGE_BYTES];
static QueueHandle_t app_rtos_event_queue;
static StaticSemaphore_t app_rtos_snapshot_mutex_control;
static SemaphoreHandle_t app_rtos_snapshot_mutex;
static app_transport_counters_t app_rtos_transport_counters;
static uint32_t app_rtos_event_queue_maximum_pending;
static app_health_policy_t app_rtos_health_policy;
static app_reset_decoded_t app_rtos_reset_reason;
app_reset_record_t app_rtos_reset_record
    __attribute__((section(".noinit.app_reset_record"), used));
static uint32_t app_rtos_watchdog_feed_count;
static bool app_rtos_reset_record_stable;
#if P5_IWDG_RESET_SMOKE_ENABLE
static bool app_rtos_iwdg_smoke_completed;
static bool app_rtos_iwdg_smoke_withholding;
#endif
static app_can_tx_scheduler_t app_rtos_can_scheduler;
static app_can_controller_t app_rtos_can_controller;
static app_can_runtime_snapshot_t app_rtos_can_snapshot;
static uint32_t app_rtos_can_next_telemetry_ms;
static uint8_t app_rtos_can_sequence;
static bool app_rtos_can_telemetry_initialized;
static bool app_rtos_can_hardware_started;
#if !APP_CAN_TX_SUPPRESSED_DIAGNOSTIC_ENABLE
static app_can_diagnostic_responder_t app_rtos_can_diagnostic_responder;
static char app_rtos_can_diagnostic_report[APP_CAN_DIAGNOSTIC_REPORT_CAPACITY];
#endif
#if P5_CAN_BOUNDED_ECHO_DIAGNOSTIC_ENABLE
static uint8_t app_rtos_can_echo_responded_mask;
#endif
#if APP_CAN_ACK_DIAGNOSTIC_ENABLE
static app_can_ack_diagnostic_t app_rtos_can_ack_diagnostic;
static char app_rtos_can_ack_report[APP_CAN_ACK_REPORT_CAPACITY];
static const p5_can_frame_t app_rtos_can_ack_tx_frame = {
    .standard_id = P5_CAN_ID_STATUS_EVENT,
    .dlc = P5_CAN_DLC,
    .data = {UINT8_C(0xA1), UINT8_C(0x5A), UINT8_C(0x54), UINT8_C(0x58),
             UINT8_C(0x4F), UINT8_C(0x4E), UINT8_C(0x43), UINT8_C(0x45)},
};
#endif
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

static uint32_t app_rtos_rs485_error_count(
    const bsp_rs485_diagnostics_t *diagnostics)
{
  uint32_t count = 0U;
  count = app_rtos_saturating_add(
      count, diagnostics->state_counters.rx_stop_failures);
  count = app_rtos_saturating_add(
      count, diagnostics->state_counters.tx_start_failures);
  count = app_rtos_saturating_add(
      count, diagnostics->state_counters.tx_timeouts);
  count = app_rtos_saturating_add(
      count, diagnostics->state_counters.uart_errors);
  count = app_rtos_saturating_add(
      count, diagnostics->state_counters.rx_rearm_failures);
  count = app_rtos_saturating_add(count, diagnostics->rx_dropped);
  count = app_rtos_saturating_add(
      count, diagnostics->irq_events.invalid_rx_lengths);
  count = app_rtos_saturating_add(
      count, diagnostics->irq_events.invalid_rx_kinds);
  count = app_rtos_saturating_add(
      count, diagnostics->irq_events.conflict_snapshots);
  return count;
}

static bool app_rtos_capture_reset_reason(void)
{
  const bool previous_smoke_request =
      app_reset_record_is_valid(&app_rtos_reset_record) &&
      (app_rtos_reset_record.last_fault_code ==
       APP_RTOS_FAULT_IWDG_SMOKE);
  app_reset_observation_t observation = {RCC->CSR, 0U};
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != RESET)
  {
    observation.normalized_flags |= APP_RESET_REASON_POWER_ON;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != RESET)
  {
    observation.normalized_flags |= APP_RESET_REASON_BROWN_OUT;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET)
  {
    observation.normalized_flags |= APP_RESET_REASON_PIN;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET)
  {
    observation.normalized_flags |= APP_RESET_REASON_SOFTWARE;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
  {
    observation.normalized_flags |= APP_RESET_REASON_IWDG;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET)
  {
    observation.normalized_flags |= APP_RESET_REASON_WWDG;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET)
  {
    observation.normalized_flags |= APP_RESET_REASON_LOW_POWER;
  }

  app_rtos_reset_reason = app_reset_reason_decode(&observation);
  if (!app_reset_record_note_boot(&app_rtos_reset_record,
                                  &app_rtos_reset_reason,
                                  APP_RTOS_FAULT_NONE))
  {
    return false;
  }
#if P5_IWDG_RESET_SMOKE_ENABLE
  app_rtos_iwdg_smoke_completed =
      previous_smoke_request &&
      (app_rtos_reset_reason.primary == APP_RESET_PRIMARY_IWDG);
#else
  (void)previous_smoke_request;
#endif
  __HAL_RCC_CLEAR_RESET_FLAGS();
  return true;
}

static void app_rtos_update_queue_watermark(void)
{
  if (app_rtos_event_queue == NULL)
  {
    return;
  }

  const uint32_t pending =
      (uint32_t)uxQueueMessagesWaiting(app_rtos_event_queue);
  taskENTER_CRITICAL();
  if (pending > app_rtos_event_queue_maximum_pending)
  {
    app_rtos_event_queue_maximum_pending = pending;
  }
  taskEXIT_CRITICAL();
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

static void app_rtos_update_measurement_snapshot(uint32_t now_ms)
{
  if (!app_bme280_get_snapshot(&app_rtos_measurement_inputs.bme280) ||
      !app_veml7700_get_snapshot(&app_rtos_measurement_inputs.veml7700) ||
      !app_adxl345_get_snapshot(&app_rtos_measurement_inputs.adxl345) ||
      !app_measurement_update(&app_rtos_measurement_model,
                              &app_rtos_measurement_inputs,
                              now_ms) ||
      !app_sensor_monitor_update(
          &app_rtos_sensor_monitor,
          &app_rtos_measurement_inputs,
          &app_rtos_measurement_model.snapshot,
          now_ms))
  {
    return;
  }

  if (app_rtos_snapshot_take(true))
  {
    (void)app_measurement_get_snapshot(&app_rtos_measurement_model,
                                       now_ms,
                                       &app_rtos_measurement_snapshot);
    (void)app_sensor_monitor_get_snapshot(
        &app_rtos_sensor_monitor, &app_rtos_sensor_monitor_snapshot);
#if P5_ADXL345_HIL_DIAGNOSTIC_ENABLE
    app_rtos_adxl345_snapshot = app_rtos_measurement_inputs.adxl345;
    app_rtos_adxl345_register_snapshot =
        app_rtos_adxl345_register_owner;
    app_rtos_adxl345_polling_snapshot =
        app_rtos_adxl345_polling_owner;
#endif
    ++app_rtos_modbus_image_generation;
    app_rtos_snapshot_give();
  }
}

static void app_rtos_acquisition_periodic_service(void)
{
  const uint32_t now_ms = (uint32_t)xTaskGetTickCount();
  app_adxl345_service(now_ms, 0U);
#if P5_ADXL345_HIL_DIAGNOSTIC_ENABLE
  app_adxl345_capture_register_diagnostic_once();
  (void)app_adxl345_get_register_diagnostic(
      &app_rtos_adxl345_register_owner);
  (void)app_adxl345_get_polling_diagnostic(
      &app_rtos_adxl345_polling_owner);
#endif
  app_bme280_service(now_ms);
  app_veml7700_service(now_ms);
  app_rtos_update_measurement_snapshot(now_ms);
}

static void app_rtos_acquisition_event_service(uint32_t event_count)
{
  if (event_count != 0U)
  {
    const uint32_t now_ms = (uint32_t)xTaskGetTickCount();
    app_adxl345_service(now_ms, event_count);
    app_rtos_update_measurement_snapshot(now_ms);
  }
}

static void app_rtos_protocol_service(void)
{
  const bsp_rs485_result_t result = bsp_rs485_poll();
#if P5_RS485_LOOPBACK_SMOKE_ENABLE
  (void)result;
  app_rs485_smoke_poll();
#else
  if (result != BSP_RS485_RESULT_OK)
  {
    app_modbus_transport_on_link_failure();
  }
  app_modbus_transport_poll();
#endif
}

static void app_rtos_protocol_event_service(void)
{
  const uint32_t event_mask = bsp_rs485_service_irq_events();
#if P5_RS485_LOOPBACK_SMOKE_ENABLE
  if (event_mask != 0U)
  {
    app_rs485_smoke_poll();
  }
#else
  if ((event_mask & BSP_RS485_IRQ_EVENT_UART_ERROR) != 0U)
  {
    app_modbus_transport_reset_partial();
  }
  app_modbus_transport_on_irq_events(event_mask);
  app_modbus_transport_service_received();
  app_modbus_transport_poll();
#endif
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
    app_rtos_irq_first_cycles = bsp_clock_cycle_now();
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

static void app_rtos_adxl345_notify_from_isr(void)
{
  TaskHandle_t acquisition_handle =
      app_rtos_task_handles[APP_TASK_ACQUISITION];
  if (acquisition_handle == NULL)
  {
    return;
  }

  BaseType_t higher_priority_task_woken = pdFALSE;
  vTaskNotifyGiveFromISR(acquisition_handle, &higher_priority_task_woken);
  portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void app_rtos_can_notify_from_isr(uint32_t event_mask)
{
  TaskHandle_t can_handle = app_rtos_task_handles[APP_TASK_CAN];
  if (can_handle == NULL)
  {
    return;
  }

  BaseType_t higher_priority_task_woken = pdFALSE;
  (void)xTaskNotifyFromISR(can_handle,
                           event_mask,
                           eSetBits,
                           &higher_priority_task_woken);
  portYIELD_FROM_ISR(higher_priority_task_woken);
}

static bool app_rtos_time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
  return (int32_t)(now_ms - deadline_ms) >= 0;
}

static uint32_t app_rtos_can_error_bits(uint32_t hal_error)
{
  uint32_t error_bits = 0U;
  if ((hal_error & HAL_CAN_ERROR_EWG) != 0U)
  {
    error_bits |= APP_CAN_ERROR_WARNING;
  }
  if ((hal_error & HAL_CAN_ERROR_EPV) != 0U)
  {
    error_bits |= APP_CAN_ERROR_PASSIVE;
  }
  if ((hal_error & HAL_CAN_ERROR_BOF) != 0U)
  {
    error_bits |= APP_CAN_ERROR_BUS_OFF;
  }
  if ((hal_error & (HAL_CAN_ERROR_TX_ALST0 |
                    HAL_CAN_ERROR_TX_ALST1 |
                    HAL_CAN_ERROR_TX_ALST2)) != 0U)
  {
    error_bits |= APP_CAN_ERROR_ARBITRATION_LOST;
  }
  if ((hal_error & HAL_CAN_ERROR_ACK) != 0U)
  {
    error_bits |= APP_CAN_ERROR_ACK;
  }
  if ((hal_error & (HAL_CAN_ERROR_STF |
                    HAL_CAN_ERROR_FOR |
                    HAL_CAN_ERROR_BR |
                    HAL_CAN_ERROR_BD |
                    HAL_CAN_ERROR_CRC |
                    HAL_CAN_ERROR_TX_TERR0 |
                    HAL_CAN_ERROR_TX_TERR1 |
                    HAL_CAN_ERROR_TX_TERR2 |
                    HAL_CAN_ERROR_TIMEOUT |
                    HAL_CAN_ERROR_NOT_INITIALIZED |
                    HAL_CAN_ERROR_NOT_READY |
                    HAL_CAN_ERROR_NOT_STARTED |
                    HAL_CAN_ERROR_PARAM |
                    HAL_CAN_ERROR_INTERNAL)) != 0U)
  {
    error_bits |= APP_CAN_ERROR_TX;
  }
  if ((hal_error & (HAL_CAN_ERROR_RX_FOV0 | HAL_CAN_ERROR_RX_FOV1)) != 0U)
  {
    error_bits |= APP_CAN_ERROR_RX_OVERRUN;
  }
  return error_bits;
}

#if APP_CAN_ACK_DIAGNOSTIC_ENABLE
static const char *app_rtos_can_ack_mode_token(void)
{
#if P5_CAN_ACK_RX_DIAGNOSTIC_ENABLE
  return "RX_ONLY";
#else
  return "TX_ONCE";
#endif
}

static const char *app_rtos_can_ack_phase_token(
    app_can_ack_diagnostic_phase_t phase)
{
  switch (phase)
  {
    case APP_CAN_ACK_PHASE_DONE:
      return "DONE";
    case APP_CAN_ACK_PHASE_TIMEOUT:
      return "TIMEOUT";
    case APP_CAN_ACK_PHASE_ERROR:
      return "ERROR";
    case APP_CAN_ACK_PHASE_ARMED:
      return "ARMED";
    case APP_CAN_ACK_PHASE_RX_LATCHED:
      return "RX_LATCHED";
    case APP_CAN_ACK_PHASE_WAIT_TX_RESULT:
      return "WAIT_TX";
    default:
      return "UNKNOWN";
  }
}

static const char *app_rtos_can_ack_send_token(
    app_can_ack_diagnostic_send_result_t result)
{
  switch (result)
  {
    case APP_CAN_ACK_SEND_OK:
      return "OK";
    case APP_CAN_ACK_SEND_BUSY:
      return "BUSY";
    case APP_CAN_ACK_SEND_ERROR:
      return "ERROR";
    case APP_CAN_ACK_SEND_NA:
    default:
      return "NA";
  }
}

static const char *app_rtos_can_controller_state_token(
    app_can_controller_state_t state)
{
  switch (state)
  {
    case APP_CAN_CONTROLLER_STOPPED:
      return "STOPPED";
    case APP_CAN_CONTROLLER_STARTING:
      return "STARTING";
    case APP_CAN_CONTROLLER_ACTIVE:
      return "ACTIVE";
    case APP_CAN_CONTROLLER_WARNING:
      return "WARNING";
    case APP_CAN_CONTROLLER_PASSIVE:
      return "PASSIVE";
    case APP_CAN_CONTROLLER_BUS_OFF:
      return "BUS_OFF";
    case APP_CAN_CONTROLLER_RECOVERY_WAIT:
      return "RECOVERY_WAIT";
    case APP_CAN_CONTROLLER_RECOVERY_LATCHED:
      return "RECOVERY_LATCHED";
    default:
      return "UNKNOWN";
  }
}

static app_can_ack_diagnostic_send_result_t
app_rtos_can_ack_map_send_result(bsp_can_send_result_t result)
{
  if (result == BSP_CAN_SEND_OK)
  {
    return APP_CAN_ACK_SEND_OK;
  }
  if (result == BSP_CAN_SEND_BUSY)
  {
    return APP_CAN_ACK_SEND_BUSY;
  }
  return APP_CAN_ACK_SEND_ERROR;
}

static void app_rtos_can_ack_initialize(uint32_t now_ms)
{
#if P5_CAN_ACK_RX_DIAGNOSTIC_ENABLE
  app_can_ack_diagnostic_initialize(&app_rtos_can_ack_diagnostic,
                                    APP_CAN_ACK_MODE_RX_ONLY,
                                    now_ms,
                                    0U,
                                    APP_CAN_ACK_RX_TIMEOUT_MS,
                                    APP_CAN_ACK_RX_REPORT_DELAY_MS);
#else
  app_can_ack_diagnostic_initialize(&app_rtos_can_ack_diagnostic,
                                    APP_CAN_ACK_MODE_TX_ONCE,
                                    now_ms,
                                    APP_CAN_ACK_TX_DELAY_MS,
                                    APP_CAN_ACK_TX_TIMEOUT_MS,
                                    0U);
  app_rtos_can_ack_diagnostic.standard_id =
      app_rtos_can_ack_tx_frame.standard_id;
  app_rtos_can_ack_diagnostic.dlc = app_rtos_can_ack_tx_frame.dlc;
  app_rtos_can_ack_diagnostic.data_xor = 0U;
  for (uint32_t index = 0U; index < P5_CAN_DLC; ++index)
  {
    app_rtos_can_ack_diagnostic.data[index] =
        app_rtos_can_ack_tx_frame.data[index];
    app_rtos_can_ack_diagnostic.data_xor ^=
        app_rtos_can_ack_tx_frame.data[index];
  }
#endif
}

static void app_rtos_can_ack_report_once(uint32_t last_hal_error)
{
  if (!app_can_ack_diagnostic_take_report(&app_rtos_can_ack_diagnostic))
  {
    return;
  }

  const bsp_can_irq_event_counters_t irq = bsp_can_irq_counters();
  CAN_HandleTypeDef *const handle = bsp_can_handle();
  const uint32_t esr = handle->Instance->ESR;
  const uint32_t tec = (esr & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos;
  const uint32_t rec = (esr & CAN_ESR_REC) >> CAN_ESR_REC_Pos;
  const uint32_t lec = (esr & CAN_ESR_LEC) >> CAN_ESR_LEC_Pos;
  const int written = snprintf(
      app_rtos_can_ack_report,
      sizeof(app_rtos_can_ack_report),
      "P5CANACK1 mode=%s phase=%s id=%03lX dlc=%lu d0=%02X d1=%02X "
      "xor=%02X rxacc=%lu rxdrop=%lu coal=%lu send=%s txc=%lu txa=%lu "
      "ack=%lu hal=%08lX state=%s esr=%08lX tec=%lu rec=%lu lec=%lu\r\n",
      app_rtos_can_ack_mode_token(),
      app_rtos_can_ack_phase_token(app_rtos_can_ack_diagnostic.phase),
      (unsigned long)app_rtos_can_ack_diagnostic.standard_id,
      (unsigned long)app_rtos_can_ack_diagnostic.dlc,
      (unsigned int)app_rtos_can_ack_diagnostic.data[0],
      (unsigned int)app_rtos_can_ack_diagnostic.data[1],
      (unsigned int)app_rtos_can_ack_diagnostic.data_xor,
      (unsigned long)irq.rx_accepted,
      (unsigned long)irq.rx_dropped,
      (unsigned long)irq.coalesced_events,
      app_rtos_can_ack_send_token(app_rtos_can_ack_diagnostic.send_result),
      (unsigned long)irq.tx_completed,
      (unsigned long)irq.tx_aborted,
      (unsigned long)app_rtos_can_controller.counters.ack_errors,
      (unsigned long)last_hal_error,
      app_rtos_can_controller_state_token(app_rtos_can_controller.state),
      (unsigned long)esr,
      (unsigned long)tec,
      (unsigned long)rec,
      (unsigned long)lec);
  if (written > 0)
  {
    const size_t report_length =
        (size_t)written < sizeof(app_rtos_can_ack_report)
            ? (size_t)written
            : (sizeof(app_rtos_can_ack_report) - 1U);
    (void)HAL_UART_Transmit(&huart2,
                            (uint8_t *)app_rtos_can_ack_report,
                            (uint16_t)report_length,
                            APP_CAN_ACK_UART_TIMEOUT_MS);
  }
}

static void app_rtos_can_ack_service(uint32_t now_ms,
                                     uint32_t last_hal_error)
{
#if P5_CAN_ACK_TX_DIAGNOSTIC_ENABLE
  if (app_can_ack_diagnostic_tx_due(&app_rtos_can_ack_diagnostic, now_ms))
  {
    const bsp_can_send_result_t result =
        bsp_can_send(&app_rtos_can_ack_tx_frame);
    app_can_ack_diagnostic_record_send_result(
        &app_rtos_can_ack_diagnostic,
        app_rtos_can_ack_map_send_result(result),
        now_ms);
  }
#endif
  app_can_ack_diagnostic_poll(&app_rtos_can_ack_diagnostic, now_ms);
  app_rtos_can_ack_report_once(last_hal_error);
}
#endif

#if P5_CAN_BOUNDED_ECHO_DIAGNOSTIC_ENABLE
static bool app_rtos_can_echo_id_bit(uint32_t standard_id, uint8_t *bit)
{
  if (bit == NULL)
  {
    return false;
  }

  switch (standard_id)
  {
    case P5_CAN_ID_STATUS_EVENT:
      *bit = UINT8_C(1) << 0U;
      return true;
    case P5_CAN_ID_HEARTBEAT:
      *bit = UINT8_C(1) << 1U;
      return true;
    case P5_CAN_ID_HEALTH_SUMMARY:
      *bit = UINT8_C(1) << 2U;
      return true;
    case P5_CAN_ID_CLIMATE_PRIMARY:
      *bit = UINT8_C(1) << 3U;
      return true;
    case P5_CAN_ID_CLIMATE_SECONDARY:
      *bit = UINT8_C(1) << 4U;
      return true;
    case P5_CAN_ID_ILLUMINANCE:
      *bit = UINT8_C(1) << 5U;
      return true;
    case P5_CAN_ID_VIBRATION_SUMMARY:
      *bit = UINT8_C(1) << 6U;
      return true;
    default:
      *bit = 0U;
      return false;
  }
}

static void app_rtos_can_bounded_echo(const bsp_can_rx_frame_t *request)
{
  uint8_t response_bit = 0U;
  if ((request == NULL) ||
      !app_rtos_can_echo_id_bit(request->standard_id, &response_bit) ||
      ((app_rtos_can_echo_responded_mask & response_bit) != 0U))
  {
    return;
  }

  app_rtos_can_echo_responded_mask |= response_bit;
  uint8_t payload_xor = 0U;
  for (uint32_t index = 0U; index < P5_CAN_DLC; ++index)
  {
    payload_xor ^= request->data[index];
  }

  const p5_can_frame_t response = {
      .standard_id = P5_CAN_ID_STATUS_EVENT,
      .dlc = P5_CAN_DLC,
      .data = {UINT8_C(0xE1),
               UINT8_C(0x00),
               (uint8_t)((request->standard_id >> 8U) & UINT32_C(0x07)),
               (uint8_t)(request->standard_id & UINT32_C(0xFF)),
               (uint8_t)request->dlc,
               request->data[0],
               request->data[1],
               payload_xor},
  };
  (void)bsp_can_send(&response);
}
#endif

#if !APP_CAN_TX_SUPPRESSED_DIAGNOSTIC_ENABLE
static size_t app_rtos_can_diagnostic_append_literal(
    char *destination,
    size_t capacity,
    size_t offset,
    const char *text)
{
  if ((destination == NULL) || (text == NULL))
  {
    return offset;
  }
  while ((*text != '\0') && (offset < capacity))
  {
    destination[offset++] = *text++;
  }
  return offset;
}

static size_t app_rtos_can_diagnostic_append_u8_decimal(
    char *destination,
    size_t capacity,
    size_t offset,
    uint8_t value)
{
  if ((destination == NULL) || (offset >= capacity))
  {
    return offset;
  }
  if (value >= UINT8_C(100))
  {
    destination[offset++] = (char)('0' + (value / UINT8_C(100)));
    value %= UINT8_C(100);
  }
  if ((value >= UINT8_C(10)) ||
      ((offset > 0U) && (destination[offset - 1U] >= '0') &&
       (destination[offset - 1U] <= '9')))
  {
    if (offset >= capacity)
    {
      return offset;
    }
    destination[offset++] = (char)('0' + (value / UINT8_C(10)));
  }
  if (offset < capacity)
  {
    destination[offset++] = (char)('0' + (value % UINT8_C(10)));
  }
  return offset;
}

static size_t app_rtos_can_diagnostic_append_u32_hex(
    char *destination,
    size_t capacity,
    size_t offset,
    uint32_t value)
{
  static const char digits[] = "0123456789ABCDEF";
  for (uint32_t nibble = 0U; nibble < 8U; ++nibble)
  {
    if (offset >= capacity)
    {
      break;
    }
    const uint32_t shift = 28U - (nibble * 4U);
    destination[offset++] = digits[(value >> shift) & UINT32_C(0x0F)];
  }
  return offset;
}

static void app_rtos_can_diagnostic_report_once(
    const p5_can_diagnostic_request_t *request,
    bool queued)
{
  if (request == NULL)
  {
    return;
  }
  size_t length = 0U;
  length = app_rtos_can_diagnostic_append_literal(
      app_rtos_can_diagnostic_report,
      sizeof(app_rtos_can_diagnostic_report),
      length,
      "P5CANDIAG1 rx=1 seq=");
  length = app_rtos_can_diagnostic_append_u8_decimal(
      app_rtos_can_diagnostic_report,
      sizeof(app_rtos_can_diagnostic_report),
      length,
      request->sequence);
  length = app_rtos_can_diagnostic_append_literal(
      app_rtos_can_diagnostic_report,
      sizeof(app_rtos_can_diagnostic_report),
      length,
      " nonce=");
  length = app_rtos_can_diagnostic_append_u32_hex(
      app_rtos_can_diagnostic_report,
      sizeof(app_rtos_can_diagnostic_report),
      length,
      request->nonce);
  length = app_rtos_can_diagnostic_append_literal(
      app_rtos_can_diagnostic_report,
      sizeof(app_rtos_can_diagnostic_report),
      length,
      queued ? " reply=QUEUED\r\n" : " reply=DROPPED\r\n");
  (void)HAL_UART_Transmit(&huart2,
                          (uint8_t *)app_rtos_can_diagnostic_report,
                          (uint16_t)length,
                          APP_CAN_DIAGNOSTIC_UART_TIMEOUT_MS);
}

static void app_rtos_can_diagnostic_request(
    const bsp_can_rx_frame_t *received,
    uint32_t now_ms)
{
  if ((received == NULL) ||
      (received->standard_id != P5_CAN_ID_DIAGNOSTIC_REQUEST) ||
      app_rtos_can_scheduler.diagnostic_pending)
  {
    return;
  }
  p5_can_frame_t request = {
      .standard_id = (uint16_t)received->standard_id,
      .dlc = (uint8_t)received->dlc,
      .data = {0U},
  };
  for (uint32_t index = 0U; index < P5_CAN_DLC; ++index)
  {
    request.data[index] = received->data[index];
  }

  p5_can_frame_t response;
  if (app_can_diagnostic_process(&app_rtos_can_diagnostic_responder,
                                 &request,
                                 now_ms,
                                 &response) != APP_CAN_DIAGNOSTIC_READY)
  {
    return;
  }
  p5_can_diagnostic_request_t decoded;
  if (p5_can_decode_diagnostic_request(&request, &decoded) !=
      P5_CAN_RESULT_OK)
  {
    return;
  }
  const bool queued = app_can_tx_publish_diagnostic_response(
      &app_rtos_can_scheduler, &response);
  app_rtos_can_diagnostic_report_once(&decoded, queued);
}
#endif

static void app_rtos_can_enqueue_event(uint16_t event_code,
                                       uint8_t severity,
                                       uint16_t detail)
{
#if APP_CAN_TX_SUPPRESSED_DIAGNOSTIC_ENABLE
  (void)event_code;
  (void)severity;
  (void)detail;
#else
  const p5_can_status_event_t payload = {
      .sequence = app_rtos_can_sequence++,
      .event_code = event_code,
      .severity = severity,
      .source = APP_CAN_EVENT_SOURCE,
      .detail = detail,
  };
  p5_can_frame_t frame;
  if (p5_can_encode_status_event(&payload, &frame) == P5_CAN_RESULT_OK)
  {
    (void)app_can_tx_enqueue_event(&app_rtos_can_scheduler,
                                   event_code,
                                   APP_CAN_EVENT_SOURCE,
                                   &frame);
  }
#endif
}

static void app_rtos_can_update_snapshot(uint32_t last_hal_error)
{
  const bsp_can_irq_event_counters_t irq = bsp_can_irq_counters();
  app_can_runtime_snapshot_t snapshot = {
      .controller_state = app_rtos_can_controller.state,
      .controller_counters = app_rtos_can_controller.counters,
      .tx_counters = app_can_tx_counters(&app_rtos_can_scheduler),
      .pending_frames = app_can_tx_pending(&app_rtos_can_scheduler),
      .last_hal_error = last_hal_error,
      .recovery_attempts_in_episode =
          app_rtos_can_controller.recovery_attempts_in_episode,
      .rx_accepted = irq.rx_accepted,
      .rx_dropped = irq.rx_dropped,
      .rx_invalid = irq.invalid_headers,
      .tx_completed = irq.tx_completed,
      .tx_aborted = irq.tx_aborted,
      .deferred_notifications = irq.deferred_notifications,
      .hardware_started = app_rtos_can_hardware_started,
  };
  if (app_rtos_snapshot_take(true))
  {
    app_rtos_can_snapshot = snapshot;
    app_rtos_snapshot_give();
  }
}

static void app_rtos_can_publish_periodic(uint32_t now_ms)
{
#if APP_CAN_TX_SUPPRESSED_DIAGNOSTIC_ENABLE
  (void)now_ms;
#else
  if (!app_rtos_can_telemetry_initialized)
  {
    app_rtos_can_next_telemetry_ms = now_ms + APP_CAN_TELEMETRY_PERIOD_MS;
    app_rtos_can_telemetry_initialized = true;
    return;
  }
  if (!app_rtos_time_reached(now_ms, app_rtos_can_next_telemetry_ms))
  {
    return;
  }
  app_rtos_can_next_telemetry_ms = now_ms + APP_CAN_TELEMETRY_PERIOD_MS;

  app_measurement_snapshot_t measurement;
  app_health_decision_t health;
  uint32_t image_generation = 0U;
  if (!app_rtos_snapshot_take(false))
  {
    return;
  }
  measurement = app_rtos_measurement_snapshot;
  health = app_rtos_health_snapshot.decision;
  image_generation = app_rtos_modbus_image_generation;
  app_rtos_snapshot_give();
  if (!app_measurement_refresh_snapshot(&measurement, now_ms))
  {
    return;
  }

  p5_can_frame_t frames[APP_CAN_PERIODIC_FRAME_COUNT];
  const uint8_t sequence = app_rtos_can_sequence++;
  if (!app_can_build_periodic_frames(&measurement,
                                     &health,
                                     image_generation,
                                     now_ms / UINT32_C(1000),
                                     sequence,
                                     frames))
  {
    return;
  }
  (void)app_can_tx_publish_telemetry(
      &app_rtos_can_scheduler,
      APP_CAN_TELEMETRY_HEARTBEAT,
      &frames[APP_CAN_PERIODIC_HEARTBEAT]);
  (void)app_can_tx_publish_telemetry(
      &app_rtos_can_scheduler,
      APP_CAN_TELEMETRY_HEALTH,
      &frames[APP_CAN_PERIODIC_HEALTH]);
  (void)app_can_tx_publish_climate_pair(
      &app_rtos_can_scheduler,
      &frames[APP_CAN_PERIODIC_CLIMATE_PRIMARY],
      &frames[APP_CAN_PERIODIC_CLIMATE_SECONDARY]);
  (void)app_can_tx_publish_telemetry(
      &app_rtos_can_scheduler,
      APP_CAN_TELEMETRY_ILLUMINANCE,
      &frames[APP_CAN_PERIODIC_ILLUMINANCE]);
  (void)app_can_tx_publish_telemetry(
      &app_rtos_can_scheduler,
      APP_CAN_TELEMETRY_VIBRATION,
      &frames[APP_CAN_PERIODIC_VIBRATION]);
#endif
}

static void app_rtos_can_drain_tx(uint32_t now_ms)
{
  for (uint32_t sent = 0U; sent < APP_CAN_TX_DRAIN_BUDGET; ++sent)
  {
    if (bsp_can_tx_free_level() == 0U)
    {
      app_can_tx_note_hal_busy(&app_rtos_can_scheduler);
      break;
    }
    p5_can_frame_t frame;
    app_can_tx_token_t token;
    if (!app_can_tx_peek(&app_rtos_can_scheduler, &frame, &token))
    {
      break;
    }
    const bsp_can_send_result_t result = bsp_can_send(&frame);
    if (result == BSP_CAN_SEND_OK)
    {
      (void)app_can_tx_commit(&app_rtos_can_scheduler, token);
      continue;
    }
    if (result == BSP_CAN_SEND_BUSY)
    {
      app_can_tx_note_hal_busy(&app_rtos_can_scheduler);
    }
    else
    {
      (void)app_can_controller_on_error(
          &app_rtos_can_controller, APP_CAN_ERROR_TX, now_ms);
    }
    break;
  }
}

static void app_rtos_can_service(uint32_t now_ms)
{
  static uint32_t last_hal_error;
  bsp_can_irq_event_snapshot_t irq;
  if (bsp_can_take_irq_snapshot(&irq))
  {
    if ((irq.event_mask & BSP_CAN_IRQ_EVENT_RX_READY) != 0U)
    {
      bsp_can_rx_frame_t frame;
      for (uint32_t received = 0U;
           (received < APP_CAN_RX_DRAIN_BUDGET) &&
           bsp_can_take_received(&frame);
           ++received)
      {
#if P5_CAN_BOUNDED_ECHO_DIAGNOSTIC_ENABLE
        app_rtos_can_bounded_echo(&frame);
#elif P5_CAN_ACK_RX_DIAGNOSTIC_ENABLE
        (void)app_can_ack_diagnostic_observe_rx(
            &app_rtos_can_ack_diagnostic,
            frame.standard_id,
            frame.dlc,
            frame.data,
            now_ms);
#elif P5_CAN_ACK_TX_DIAGNOSTIC_ENABLE
        (void)frame;
#else
        app_rtos_can_diagnostic_request(&frame, now_ms);
#endif
      }
    }
    uint32_t error_bits = 0U;
    if ((irq.event_mask & BSP_CAN_IRQ_EVENT_ERROR) != 0U)
    {
      last_hal_error = irq.latest_hal_error;
      error_bits |= app_rtos_can_error_bits(irq.latest_hal_error);
    }
    if ((irq.event_mask & BSP_CAN_IRQ_EVENT_TX_ABORT) != 0U)
    {
      error_bits |= APP_CAN_ERROR_TX;
    }
    if (error_bits != 0U)
    {
      const bool state_changed = app_can_controller_on_error(
          &app_rtos_can_controller, error_bits, now_ms);
      if ((error_bits & APP_CAN_ERROR_BUS_OFF) != 0U)
      {
        app_rtos_can_hardware_started = false;
        if (state_changed)
        {
          app_rtos_can_enqueue_event(
              APP_CAN_EVENT_BUS_OFF, UINT8_C(2), (uint16_t)last_hal_error);
        }
      }
    }
#if APP_CAN_ACK_DIAGNOSTIC_ENABLE
    if (error_bits != 0U)
    {
      uint32_t diagnostic_error = APP_CAN_ACK_DIAGNOSTIC_ERROR_TX;
      if ((error_bits & APP_CAN_ERROR_ACK) != 0U)
      {
        diagnostic_error |= APP_CAN_ACK_DIAGNOSTIC_ERROR_ACK;
      }
      app_can_ack_diagnostic_record_error(
          &app_rtos_can_ack_diagnostic, diagnostic_error, now_ms);
    }
    else if ((irq.event_mask & BSP_CAN_IRQ_EVENT_TX_ABORT) != 0U)
    {
      app_can_ack_diagnostic_record_tx_abort(
          &app_rtos_can_ack_diagnostic, now_ms);
    }
    else if ((irq.event_mask & BSP_CAN_IRQ_EVENT_TX_COMPLETE) != 0U)
    {
      app_can_ack_diagnostic_record_tx_complete(
          &app_rtos_can_ack_diagnostic, now_ms);
    }
#endif
  }

  if (app_can_controller_recovery_due(&app_rtos_can_controller, now_ms))
  {
    (void)bsp_can_stop();
    if (app_can_controller_begin_recovery(&app_rtos_can_controller, now_ms))
    {
      const bool started = bsp_can_start();
      app_rtos_can_hardware_started = started;
      (void)app_can_controller_complete_start(
          &app_rtos_can_controller, started, now_ms);
      app_rtos_can_enqueue_event(
          started ? APP_CAN_EVENT_RECOVERED : APP_CAN_EVENT_START_FAILED,
          started ? UINT8_C(1) : UINT8_C(2),
          (uint16_t)HAL_CAN_GetError(bsp_can_handle()));
    }
  }

#if APP_CAN_ACK_DIAGNOSTIC_ENABLE
  app_rtos_can_ack_service(now_ms, last_hal_error);
#elif !P5_CAN_BOUNDED_ECHO_DIAGNOSTIC_ENABLE
  app_rtos_can_publish_periodic(now_ms);
  if ((app_rtos_can_controller.state == APP_CAN_CONTROLLER_ACTIVE) ||
      (app_rtos_can_controller.state == APP_CAN_CONTROLLER_WARNING) ||
      (app_rtos_can_controller.state == APP_CAN_CONTROLLER_PASSIVE))
  {
    app_rtos_can_drain_tx(now_ms);
  }
#endif
  app_rtos_can_update_snapshot(last_hal_error);
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
    task_cycles = bsp_clock_cycle_now();
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

static void app_rtos_watchdog_service(
    const app_health_decision_t *decision)
{
  const bool feed_allowed =
      decision->feed_decision == APP_WATCHDOG_FEED_ALLOWED;

#if P5_IWDG_RESET_SMOKE_ENABLE
  if (feed_allowed && !app_rtos_iwdg_smoke_completed &&
      !app_rtos_iwdg_smoke_withholding &&
      (app_rtos_watchdog_feed_count >= APP_IWDG_SMOKE_PRIME_FEEDS))
  {
    app_rtos_current_fault_code = APP_RTOS_FAULT_IWDG_SMOKE;
    if (!app_reset_record_note_fault(&app_rtos_reset_record,
                                     APP_RTOS_FAULT_IWDG_SMOKE))
    {
      app_rtos_fail_stop(APP_RTOS_FAULT_MODEL);
    }
    app_boot_report_iwdg_withhold();
    app_rtos_iwdg_smoke_withholding = true;
  }
  if (app_rtos_iwdg_smoke_withholding)
  {
    return;
  }
#endif

  if (!feed_allowed)
  {
    return;
  }
  if (!bsp_watchdog_refresh())
  {
    app_rtos_fail_stop(APP_RTOS_FAULT_IWDG_REFRESH);
  }
  app_rtos_watchdog_feed_count = app_rtos_saturating_add(
      app_rtos_watchdog_feed_count, 1U);
  if (!app_rtos_reset_record_stable &&
      (app_rtos_watchdog_feed_count >= APP_IWDG_STABLE_FEEDS))
  {
    if (!app_reset_record_note_stable(&app_rtos_reset_record))
    {
      app_rtos_fail_stop(APP_RTOS_FAULT_MODEL);
    }
    app_rtos_reset_record_stable = true;
  }
}

static void app_rtos_health_service(void)
{
  app_rtos_health_snapshot_t snapshot = {0};
  app_health_input_t input = {0};
  app_transport_counters_t transport_counters;
  uint32_t maximum_pending = 0U;

  taskENTER_CRITICAL();
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    snapshot.task[index].release_count =
        app_rtos_task_runtime[index].release_count;
    snapshot.task[index].missed_release_count =
        app_rtos_task_runtime[index].missed_release_count;
    snapshot.task[index].deadline_miss_count =
        app_rtos_task_runtime[index].deadline_miss_count;
    snapshot.task[index].budget_overrun_count =
        app_rtos_task_runtime[index].budget_overrun_count;
    snapshot.release_count = app_rtos_saturating_add(
        snapshot.release_count, snapshot.task[index].release_count);
    snapshot.missed_release_count = app_rtos_saturating_add(
        snapshot.missed_release_count,
        snapshot.task[index].missed_release_count);
    snapshot.deadline_miss_count = app_rtos_saturating_add(
        snapshot.deadline_miss_count,
        snapshot.task[index].deadline_miss_count);
    snapshot.budget_overrun_count = app_rtos_saturating_add(
        snapshot.budget_overrun_count,
        snapshot.task[index].budget_overrun_count);
    input.task_release_count[index] = snapshot.task[index].release_count;
    input.task_deadline_miss_count[index] =
        snapshot.task[index].deadline_miss_count;
    input.task_budget_overrun_count[index] =
        snapshot.task[index].budget_overrun_count;
  }
  transport_counters = app_rtos_transport_counters;
  maximum_pending = app_rtos_event_queue_maximum_pending;
  taskEXIT_CRITICAL();

  input.queue_dropped_count = transport_counters.event_dropped_full_count;
  input.queue_current_pending =
      (uint32_t)uxQueueMessagesWaiting(app_rtos_event_queue);
  input.queue_maximum_pending = maximum_pending;
  input.queue_depth = APP_TRANSPORT_EVENT_QUEUE_DEPTH;
  input.snapshot_contention_count = app_rtos_saturating_add(
      transport_counters.snapshot_read_contention_count,
      transport_counters.snapshot_write_contention_count);
  const bsp_rs485_diagnostics_t rs485_diagnostics =
      bsp_rs485_get_diagnostics();
  input.rs485_error_count = app_rtos_rs485_error_count(&rs485_diagnostics);
  app_sensor_monitor_snapshot_t sensor_monitor;
  if (app_rtos_get_sensor_monitor_snapshot(&sensor_monitor))
  {
    input.sensor_unavailable_mask =
        sensor_monitor.unavailable_device_mask;
    input.sensor_stale_mask = sensor_monitor.stale_source_mask;
    input.sensor_recovery_mask = sensor_monitor.recovery_device_mask;
  }
  input.recovery_result = APP_HEALTH_RECOVERY_NONE;
  input.reset_loop_latched =
      app_reset_record_loop_latched(&app_rtos_reset_record);

  if (!app_health_policy_evaluate(
          &app_rtos_health_policy, &input, &snapshot.decision))
  {
    app_rtos_fail_stop(APP_RTOS_FAULT_MODEL);
  }
  snapshot.rs485_error_count = input.rs485_error_count;
  app_rtos_watchdog_service(&snapshot.decision);

  if (app_rtos_snapshot_take(true))
  {
    app_rtos_health_snapshot = snapshot;
    ++app_rtos_modbus_image_generation;
    app_rtos_snapshot_give();
  }

  app_rtos_update_resource_snapshot();

  if (snapshot.decision.publish_transition)
  {
    const uint16_t code = app_health_state_event_code(
        snapshot.decision.state);
    if (code != 0U)
    {
      const app_transport_event_t event = {
          (uint32_t)xTaskGetTickCount(),
          (snapshot.decision.warning_mask & UINT32_C(0x0000ffff)) |
              ((snapshot.decision.stalled_task_mask & UINT32_C(0x0000ffff))
               << 16U),
          APP_HEALTH_EVENT_SOURCE,
          code,
      };
      (void)app_rtos_publish_diagnostic_event(&event);
    }
  }
}

#if P5_ADXL345_HIL_DIAGNOSTIC_ENABLE
static const char *app_rtos_adxl345_status_token(adxl345_status_t status)
{
  switch (status)
  {
    case ADXL345_STATUS_UNINITIALIZED:
      return "UNINIT";
    case ADXL345_STATUS_INITIALIZING:
      return "INIT";
    case ADXL345_STATUS_VALID:
      return "VALID";
    case ADXL345_STATUS_WRONG_ID:
      return "WRONG_ID";
    case ADXL345_STATUS_TRANSPORT_INVALID_ARGUMENT:
      return "ARG";
    case ADXL345_STATUS_TRANSPORT_BUSY:
      return "BUSY";
    case ADXL345_STATUS_TRANSPORT_TIMEOUT:
      return "TIMEOUT";
    case ADXL345_STATUS_TRANSPORT_IO_ERROR:
      return "IO_ERROR";
    case ADXL345_STATUS_CONFIGURATION_MISMATCH:
      return "CFG_ERROR";
    case ADXL345_STATUS_DATA_READY_STALLED:
      return "STALLED";
    case ADXL345_STATUS_RECOVERY_REQUIRED:
      return "RECOVERY";
    case ADXL345_STATUS_OFFLINE:
      return "OFFLINE";
    default:
      return "UNKNOWN";
  }
}

static const char *app_rtos_adxl345_state_token(adxl345_state_t state)
{
  switch (state)
  {
    case ADXL345_STATE_UNINITIALIZED:
      return "UNINIT";
    case ADXL345_STATE_READ_ID:
      return "READ_ID";
    case ADXL345_STATE_WRITE_STANDBY:
      return "WRITE_STANDBY";
    case ADXL345_STATE_DISABLE_INTERRUPTS:
      return "INT_OFF";
    case ADXL345_STATE_SET_FIFO_BYPASS:
      return "FIFO_BYPASS";
    case ADXL345_STATE_SET_DATA_FORMAT:
      return "SET_FORMAT";
    case ADXL345_STATE_SET_BW_RATE:
      return "SET_RATE";
    case ADXL345_STATE_MAP_INT1:
      return "MAP_INT1";
    case ADXL345_STATE_VERIFY_DATA_FORMAT:
      return "VERIFY_FORMAT";
    case ADXL345_STATE_VERIFY_BW_RATE:
      return "VERIFY_RATE";
    case ADXL345_STATE_VERIFY_INT_MAP:
      return "VERIFY_MAP";
    case ADXL345_STATE_ENTER_MEASURE:
      return "ENTER_MEASURE";
    case ADXL345_STATE_VERIFY_POWER_CTL:
      return "VERIFY_POWER";
    case ADXL345_STATE_ENABLE_DATA_READY:
      return "ENABLE_DRDY";
    case ADXL345_STATE_WAIT_DATA_READY:
      return "WAIT_DRDY";
    case ADXL345_STATE_OFFLINE:
      return "OFFLINE";
    default:
      return "UNKNOWN";
  }
}

static const char *app_rtos_adxl345_transport_token(
    adxl345_transport_result_t result)
{
  switch (result)
  {
    case ADXL345_TRANSPORT_OK:
      return "OK";
    case ADXL345_TRANSPORT_INVALID_ARGUMENT:
      return "ARG";
    case ADXL345_TRANSPORT_BUSY:
      return "BUSY";
    case ADXL345_TRANSPORT_TIMEOUT:
      return "TIMEOUT";
    case ADXL345_TRANSPORT_IO_ERROR:
      return "IO_ERROR";
    default:
      return "UNKNOWN";
  }
}

static void app_rtos_adxl345_hil_diagnostic_service(void)
{
  if (app_rtos_adxl345_hil_report_count >= APP_ADXL345_HIL_REPORT_LIMIT)
  {
    return;
  }

  const uint32_t now_ms = (uint32_t)xTaskGetTickCount();
  if (!bsp_clock_interval_elapsed(now_ms,
                                  app_rtos_adxl345_hil_last_report_ms,
                                  APP_ADXL345_HIL_REPORT_INTERVAL_MS))
  {
    return;
  }

  app_adxl345_snapshot_t snapshot;
  app_adxl345_register_diagnostic_t registers;
  app_adxl345_polling_diagnostic_t polling;
  if (!app_rtos_snapshot_take(false))
  {
    return;
  }
  snapshot = app_rtos_adxl345_snapshot;
  registers = app_rtos_adxl345_register_snapshot;
  polling = app_rtos_adxl345_polling_snapshot;
  app_rtos_snapshot_give();
  const unsigned int int1_level =
      HAL_GPIO_ReadPin(ADXL345_INT1_GPIO_Port, ADXL345_INT1_Pin) ==
              GPIO_PIN_SET
          ? 1U
          : 0U;

  const int written = snprintf(
      app_rtos_adxl345_hil_report,
      sizeof(app_rtos_adxl345_hil_report),
      "P5ADXL1 t=%lu st=%s sm=%s last=%s tr=%s txn=%lu int1=%u "
      "regs=%02X/%02X/%02X/%02X regok=%u "
      "poll=%lu/%lu/%lu psrc=%02X "
      "sseq=%lu irq=%lu drop=%lu "
      "x=%ld y=%ld z=%ld fseq=%lu rms=%lu/%lu/%lu "
      "peak=%lu/%lu/%lu rrms=%lu err=%lu rec=%lu/%lu\r\n",
      (unsigned long)now_ms,
      app_rtos_adxl345_status_token(snapshot.status),
      app_rtos_adxl345_state_token(snapshot.state),
      app_rtos_adxl345_status_token(snapshot.last_error_status),
      app_rtos_adxl345_transport_token(snapshot.last_transport_result),
      (unsigned long)snapshot.transaction_count,
      int1_level,
      (unsigned int)registers.power_ctl,
      (unsigned int)registers.int_enable,
      (unsigned int)registers.int_map,
      (unsigned int)registers.int_source,
      registers.attempted && registers.valid ? 1U : 0U,
      (unsigned long)polling.attempt_count,
      (unsigned long)polling.ready_count,
      (unsigned long)polling.error_count,
      (unsigned int)polling.last_int_source,
      (unsigned long)snapshot.sample.sequence,
      (unsigned long)snapshot.irq_event_count,
      (unsigned long)snapshot.dropped_sample_lower_bound,
      (long)snapshot.sample.acceleration_millig[0],
      (long)snapshot.sample.acceleration_millig[1],
      (long)snapshot.sample.acceleration_millig[2],
      (unsigned long)snapshot.feature.sequence,
      (unsigned long)snapshot.feature.rms_millig[0],
      (unsigned long)snapshot.feature.rms_millig[1],
      (unsigned long)snapshot.feature.rms_millig[2],
      (unsigned long)snapshot.feature.peak_abs_millig[0],
      (unsigned long)snapshot.feature.peak_abs_millig[1],
      (unsigned long)snapshot.feature.peak_abs_millig[2],
      (unsigned long)snapshot.feature.resultant_rms_millig,
      (unsigned long)snapshot.error_count,
      (unsigned long)snapshot.recovery_request_count,
      (unsigned long)snapshot.recovery_success_count);

  app_rtos_adxl345_hil_last_report_ms = now_ms;
  ++app_rtos_adxl345_hil_report_count;
  if (written > 0)
  {
    const size_t report_length =
        (size_t)written < sizeof(app_rtos_adxl345_hil_report)
            ? (size_t)written
            : (sizeof(app_rtos_adxl345_hil_report) - 1U);
    (void)HAL_UART_Transmit(&huart2,
                            (uint8_t *)app_rtos_adxl345_hil_report,
                            (uint16_t)report_length,
                            APP_ADXL345_HIL_UART_TIMEOUT_MS);
  }
}
#endif

#if P5_SOAK_DIAGNOSTIC_ENABLE
static void app_rtos_soak_diagnostic_service(void)
{
  const uint32_t now_ms = (uint32_t)xTaskGetTickCount();
  if (app_rtos_soak_diagnostic_has_report &&
      !bsp_clock_interval_elapsed(now_ms,
                                  app_rtos_soak_diagnostic_last_report_ms,
                                  APP_SOAK_DIAGNOSTIC_INTERVAL_MS))
  {
    return;
  }

  if (!app_rtos_get_health_snapshot(&app_rtos_soak_health) ||
      !app_rtos_get_resource_snapshot(&app_rtos_soak_resource) ||
      !app_rtos_get_transport_snapshot(&app_rtos_soak_transport) ||
      !app_rtos_get_can_snapshot(&app_rtos_soak_can) ||
      !app_rtos_get_measurement_snapshot(&app_rtos_soak_measurement) ||
      !app_rtos_get_sensor_monitor_snapshot(&app_rtos_soak_monitor) ||
      !app_rtos_get_reset_reason(&app_rtos_soak_reset) ||
      !app_rtos_get_reset_record(&app_rtos_soak_record))
  {
    return;
  }

  app_rtos_soak_modbus = app_modbus_transport_get_diagnostics();
  app_rtos_soak_snapshot = (app_soak_diagnostic_snapshot_t){0};
  app_rtos_soak_snapshot.now_ms = now_ms;
  app_rtos_soak_snapshot.boot_count = app_rtos_soak_record.boot_count;
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    app_rtos_soak_snapshot.task[index].release =
        app_rtos_soak_health.task[index].release_count;
    app_rtos_soak_snapshot.task[index].missed =
        app_rtos_soak_health.task[index].missed_release_count;
    app_rtos_soak_snapshot.task[index].deadline_miss =
        app_rtos_soak_health.task[index].deadline_miss_count;
    app_rtos_soak_snapshot.task[index].budget_overrun =
        app_rtos_soak_health.task[index].budget_overrun_count;
    app_rtos_soak_snapshot.task[index].configured_words =
        app_rtos_soak_resource.task[index].configured_words;
    app_rtos_soak_snapshot.task[index].minimum_free_words =
        app_rtos_soak_resource.task[index].minimum_free_words;
    app_rtos_soak_snapshot.task[index].measured =
        app_rtos_soak_resource.task[index].measured;
  }
  app_rtos_soak_snapshot.queue_current = app_rtos_soak_transport.current_pending;
  app_rtos_soak_snapshot.queue_maximum = app_rtos_soak_transport.maximum_pending;
  app_rtos_soak_snapshot.queue_depth = app_rtos_soak_transport.depth;
  app_rtos_soak_snapshot.queue_dropped =
      app_rtos_soak_transport.counters.event_dropped_full_count;
  app_rtos_soak_snapshot.queue_drained =
      app_rtos_soak_transport.counters.event_drained_count;
  app_rtos_soak_snapshot.health_state =
      (uint32_t)app_rtos_soak_health.decision.state;
  app_rtos_soak_snapshot.health_warning_mask =
      app_rtos_soak_health.decision.warning_mask;
  app_rtos_soak_snapshot.health_stalled_mask =
      app_rtos_soak_health.decision.stalled_task_mask;
  app_rtos_soak_snapshot.watchdog_feed =
      (uint32_t)app_rtos_soak_health.decision.feed_decision;
  app_rtos_soak_snapshot.fault_code = app_rtos_fault_code();
  app_rtos_soak_snapshot.reset_primary =
      (uint32_t)app_rtos_soak_reset.primary;
  app_rtos_soak_snapshot.reset_raw_flags =
      app_rtos_soak_reset.hardware_raw_flags;
  app_rtos_soak_snapshot.reset_loop =
      app_reset_record_loop_latched(&app_rtos_soak_record);
  app_rtos_soak_snapshot.rs485_accepted =
      app_rtos_soak_modbus.server.addressed_requests;
  app_rtos_soak_snapshot.rs485_error_count =
      app_rtos_soak_health.rs485_error_count;
  app_rtos_soak_snapshot.can_state =
      (uint32_t)app_rtos_soak_can.controller_state;
  app_rtos_soak_snapshot.can_pending = app_rtos_soak_can.pending_frames;
  app_rtos_soak_snapshot.can_capacity = APP_SOAK_CAN_CAPACITY;
  app_rtos_soak_snapshot.can_maximum_pending =
      app_rtos_soak_can.tx_counters.maximum_pending;
  app_rtos_soak_snapshot.can_event_dropped =
      app_rtos_soak_can.tx_counters.event_dropped;
  app_rtos_soak_snapshot.can_hal_busy =
      app_rtos_soak_can.tx_counters.hal_busy;
  app_rtos_soak_snapshot.can_bus_off =
      app_rtos_soak_can.controller_counters.bus_off_transitions;
  app_rtos_soak_snapshot.can_recovery_attempts =
      app_rtos_soak_can.controller_counters.recovery_attempts;

  app_rtos_soak_snapshot.sensor[0].state =
      (uint32_t)app_rtos_soak_measurement.bme280.metadata.state;
  app_rtos_soak_snapshot.sensor[0].sequence =
      app_rtos_soak_measurement.bme280.metadata.sequence;
  app_rtos_soak_snapshot.sensor[0].fault_count =
      app_rtos_soak_monitor.device[APP_SENSOR_DEVICE_BME280].fault_episode_count;
  app_rtos_soak_snapshot.sensor[0].recovery_count = app_rtos_soak_monitor
      .device[APP_SENSOR_DEVICE_BME280]
      .recovery_success_count;
  app_rtos_soak_snapshot.sensor[1].state =
      (uint32_t)app_rtos_soak_measurement.veml7700.metadata.state;
  app_rtos_soak_snapshot.sensor[1].sequence =
      app_rtos_soak_measurement.veml7700.metadata.sequence;
  app_rtos_soak_snapshot.sensor[1].fault_count = app_rtos_soak_monitor
      .device[APP_SENSOR_DEVICE_VEML7700]
      .fault_episode_count;
  app_rtos_soak_snapshot.sensor[1].recovery_count = app_rtos_soak_monitor
      .device[APP_SENSOR_DEVICE_VEML7700]
      .recovery_success_count;
  app_rtos_soak_snapshot.sensor[2].state =
      (uint32_t)app_rtos_soak_measurement.adxl345_sample.metadata.state;
  app_rtos_soak_snapshot.sensor[2].sequence =
      app_rtos_soak_measurement.adxl345_sample.metadata.sequence;
  app_rtos_soak_snapshot.sensor[2].fault_count = app_rtos_soak_monitor
      .device[APP_SENSOR_DEVICE_ADXL345]
      .fault_episode_count;
  app_rtos_soak_snapshot.sensor[2].recovery_count = app_rtos_soak_monitor
      .device[APP_SENSOR_DEVICE_ADXL345]
      .recovery_success_count;
  app_rtos_soak_snapshot.sensor[3].state =
      (uint32_t)app_rtos_soak_measurement.adxl345_feature.metadata.state;
  app_rtos_soak_snapshot.sensor[3].sequence =
      app_rtos_soak_measurement.adxl345_feature.metadata.sequence;
  app_rtos_soak_snapshot.sensor[3].fault_count =
      app_rtos_soak_snapshot.sensor[2].fault_count;
  app_rtos_soak_snapshot.sensor[3].recovery_count =
      app_rtos_soak_snapshot.sensor[2].recovery_count;

  size_t report_length = 0U;
  if (app_soak_diagnostic_format(&app_rtos_soak_snapshot,
                                 app_rtos_soak_diagnostic_report,
                                 sizeof(app_rtos_soak_diagnostic_report),
                                 &report_length))
  {
    (void)HAL_UART_Transmit(&huart2,
                            (uint8_t *)app_rtos_soak_diagnostic_report,
                            (uint16_t)report_length,
                            APP_SOAK_DIAGNOSTIC_UART_TIMEOUT_MS);
    app_rtos_soak_diagnostic_last_report_ms = now_ms;
    app_rtos_soak_diagnostic_has_report = true;
  }
}
#endif

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
#if P5_ADXL345_HIL_DIAGNOSTIC_ENABLE
  app_rtos_adxl345_hil_diagnostic_service();
#endif
#if P5_SOAK_DIAGNOSTIC_ENABLE
  app_rtos_soak_diagnostic_service();
#endif
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

    TickType_t wait_ticks = (TickType_t)
        app_task_runtime_ticks_until_release(runtime, (uint32_t)now_tick);
#if !P5_RS485_LOOPBACK_SMOKE_ENABLE
    if (app_modbus_transport_has_partial_frame() &&
        (wait_ticks > (TickType_t)1U))
    {
      wait_ticks = (TickType_t)1U;
    }
#endif
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

static _Noreturn void app_rtos_run_acquisition(void)
{
  const app_task_contract_t *contract =
      app_task_model_contract(APP_TASK_ACQUISITION);
  if (contract == NULL)
  {
    app_rtos_fail_stop(APP_RTOS_FAULT_MODEL);
  }

  app_task_runtime_t *runtime =
      &app_rtos_task_runtime[APP_TASK_ACQUISITION];
  app_task_runtime_initialize(runtime, (uint32_t)xTaskGetTickCount());

  for (;;)
  {
    TickType_t now_tick = xTaskGetTickCount();
    if (app_task_runtime_release_due(runtime, (uint32_t)now_tick))
    {
      const TickType_t actual_start_tick = now_tick;
      app_rtos_acquisition_periodic_service();
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
    const uint32_t event_count = ulTaskNotifyTake(pdTRUE, wait_ticks);
    app_rtos_acquisition_event_service(event_count);
  }
}

static _Noreturn void app_rtos_run_can(void)
{
  const app_task_contract_t *contract =
      app_task_model_contract(APP_TASK_CAN);
  if (contract == NULL)
  {
    app_rtos_fail_stop(APP_RTOS_FAULT_MODEL);
  }

  app_task_runtime_t *runtime = &app_rtos_task_runtime[APP_TASK_CAN];
  uint32_t now_ms = (uint32_t)xTaskGetTickCount();
  app_task_runtime_initialize(runtime, now_ms);
#if APP_CAN_ACK_DIAGNOSTIC_ENABLE
  app_rtos_can_ack_initialize(now_ms);
#endif
  if (!app_can_controller_begin_initial_start(&app_rtos_can_controller))
  {
    app_rtos_fail_stop(APP_RTOS_FAULT_MODEL);
  }
  const bool started = bsp_can_start();
  app_rtos_can_hardware_started = started;
  (void)app_can_controller_complete_start(
      &app_rtos_can_controller, started, now_ms);
  if (!started)
  {
#if APP_CAN_ACK_DIAGNOSTIC_ENABLE
    app_can_ack_diagnostic_record_error(
        &app_rtos_can_ack_diagnostic,
        APP_CAN_ACK_DIAGNOSTIC_ERROR_TX,
        now_ms);
#endif
    app_rtos_can_enqueue_event(
        APP_CAN_EVENT_START_FAILED,
        UINT8_C(2),
        (uint16_t)HAL_CAN_GetError(bsp_can_handle()));
  }

  for (;;)
  {
    now_ms = (uint32_t)xTaskGetTickCount();
    if (app_task_runtime_release_due(runtime, now_ms))
    {
      const TickType_t actual_start_tick = (TickType_t)now_ms;
      app_rtos_can_service(now_ms);
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
        app_task_runtime_ticks_until_release(runtime, now_ms);
    uint32_t notification_value = 0U;
    if (xTaskNotifyWait(0U,
                        UINT32_MAX,
                        &notification_value,
                        wait_ticks) == pdTRUE)
    {
      (void)notification_value;
      app_rtos_can_service((uint32_t)xTaskGetTickCount());
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
  app_rtos_run_acquisition();
}

static void app_rtos_can_task(void *context)
{
  (void)context;
  app_rtos_run_can();
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

  app_rtos_watchdog_feed_count = 0U;
  app_rtos_reset_record_stable = false;
#if P5_ADXL345_HIL_DIAGNOSTIC_ENABLE
  app_rtos_adxl345_hil_last_report_ms = 0U;
  app_rtos_adxl345_hil_report_count = 0U;
  app_rtos_adxl345_register_owner =
      (app_adxl345_register_diagnostic_t){0};
  app_rtos_adxl345_register_snapshot =
      (app_adxl345_register_diagnostic_t){0};
  app_rtos_adxl345_polling_owner =
      (app_adxl345_polling_diagnostic_t){0};
  app_rtos_adxl345_polling_snapshot =
      (app_adxl345_polling_diagnostic_t){0};
#endif
#if P5_IWDG_RESET_SMOKE_ENABLE
  app_rtos_iwdg_smoke_completed = false;
  app_rtos_iwdg_smoke_withholding = false;
#endif
  bsp_watchdog_enable_debug_freeze();
  if (!app_rtos_capture_reset_reason())
  {
    return APP_RTOS_ERROR;
  }
#if P5_IWDG_RESET_SMOKE_ENABLE
  if (app_rtos_iwdg_smoke_completed)
  {
    app_boot_report_iwdg_reset_ok();
  }
#endif

  app_transport_counters_initialize(&app_rtos_transport_counters);
  app_rtos_event_queue_maximum_pending = 0U;
  app_rtos_modbus_image_generation = 0U;
  app_health_policy_initialize(&app_rtos_health_policy);
  app_measurement_model_initialize(&app_rtos_measurement_model);
  app_sensor_monitor_initialize(&app_rtos_sensor_monitor);
  app_can_tx_scheduler_initialize(&app_rtos_can_scheduler);
#if !APP_CAN_TX_SUPPRESSED_DIAGNOSTIC_ENABLE
  app_can_diagnostic_initialize(&app_rtos_can_diagnostic_responder);
#endif
  app_can_controller_initialize(&app_rtos_can_controller);
  app_rtos_can_snapshot = (app_can_runtime_snapshot_t){0};
  app_rtos_can_next_telemetry_ms = 0U;
  app_rtos_can_sequence = 0U;
  app_rtos_can_telemetry_initialized = false;
  app_rtos_can_hardware_started = false;
  bsp_can_irq_initialize();
  bsp_adxl345_irq_initialize();
  app_adxl345_initialize();
  app_bme280_initialize();
  app_veml7700_initialize();
  app_rtos_update_measurement_snapshot(0U);
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
  bsp_adxl345_register_irq_notifier(app_rtos_adxl345_notify_from_isr);
  bsp_can_register_irq_notifier(app_rtos_can_notify_from_isr);

  return APP_RTOS_OK;
}

bool app_rtos_publish_diagnostic_event(const app_transport_event_t *event)
{
  BaseType_t queued = pdFALSE;
  if ((app_rtos_event_queue != NULL) && app_transport_event_is_valid(event))
  {
    queued = xQueueSend(app_rtos_event_queue, event, 0U);
    if (queued == pdTRUE)
    {
      app_rtos_update_queue_watermark();
    }
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

bool app_rtos_get_can_snapshot(app_can_runtime_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return false;
  }
  if (!app_rtos_snapshot_take(false))
  {
    return false;
  }
  *snapshot = app_rtos_can_snapshot;
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

bool app_rtos_get_measurement_snapshot(
    app_measurement_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return false;
  }

  if (!app_rtos_snapshot_take(false))
  {
    return false;
  }
  *snapshot = app_rtos_measurement_snapshot;
  app_rtos_snapshot_give();
  return app_measurement_refresh_snapshot(
      snapshot, (uint32_t)xTaskGetTickCount());
}

bool app_rtos_get_sensor_monitor_snapshot(
    app_sensor_monitor_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return false;
  }

  if (!app_rtos_snapshot_take(false))
  {
    return false;
  }
  *snapshot = app_rtos_sensor_monitor_snapshot;
  app_rtos_snapshot_give();
  return snapshot->schema_revision == APP_SENSOR_MONITOR_SCHEMA_REVISION;
}

bool app_rtos_get_modbus_register_source(
    app_modbus_register_source_t *source)
{
  if (source == NULL)
  {
    return false;
  }
  if (!app_rtos_snapshot_take(false))
  {
    return false;
  }

  source->register_image_generation = app_rtos_modbus_image_generation;
  source->measurement = app_rtos_measurement_snapshot;
  source->sensor_monitor_schema_revision =
      app_rtos_sensor_monitor_snapshot.schema_revision;
  source->sensor_unavailable_mask =
      app_rtos_sensor_monitor_snapshot.unavailable_device_mask;
  source->source_stale_mask =
      app_rtos_sensor_monitor_snapshot.stale_source_mask;
  source->sensor_recovery_mask =
      app_rtos_sensor_monitor_snapshot.recovery_device_mask;
  source->adxl345_irq_event_count =
      app_rtos_sensor_monitor_snapshot.adxl345_irq_event_count;
  source->adxl345_dropped_sample_lower_bound =
      app_rtos_sensor_monitor_snapshot.adxl345_dropped_sample_lower_bound;
  for (size_t index = 0U; index < APP_SENSOR_DEVICE_COUNT; ++index)
  {
    source->device[index].fault_class =
        app_rtos_sensor_monitor_snapshot.device[index].fault_class;
    source->device[index].fault_episode_count =
        app_rtos_sensor_monitor_snapshot.device[index].fault_episode_count;
    source->device[index].recovery_request_count =
        app_rtos_sensor_monitor_snapshot.device[index].recovery_request_count;
    source->device[index].recovery_success_count =
        app_rtos_sensor_monitor_snapshot.device[index].recovery_success_count;
  }
  source->health_state = app_rtos_health_snapshot.decision.state;
  source->health_warning_mask = app_rtos_health_snapshot.decision.warning_mask;
  source->rs485_error_count = app_rtos_health_snapshot.rs485_error_count;
  app_rtos_snapshot_give();

  if (!app_measurement_refresh_snapshot(
          &source->measurement, (uint32_t)xTaskGetTickCount()))
  {
    return false;
  }
  source->source_stale_mask = 0U;
  if (source->measurement.bme280.metadata.state ==
      APP_MEASUREMENT_STATE_STALE)
  {
    source->source_stale_mask |=
        UINT32_C(1) << APP_MEASUREMENT_SOURCE_BME280;
  }
  if (source->measurement.veml7700.metadata.state ==
      APP_MEASUREMENT_STATE_STALE)
  {
    source->source_stale_mask |=
        UINT32_C(1) << APP_MEASUREMENT_SOURCE_VEML7700;
  }
  if (source->measurement.adxl345_sample.metadata.state ==
      APP_MEASUREMENT_STATE_STALE)
  {
    source->source_stale_mask |=
        UINT32_C(1) << APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE;
  }
  if (source->measurement.adxl345_feature.metadata.state ==
      APP_MEASUREMENT_STATE_STALE)
  {
    source->source_stale_mask |=
        UINT32_C(1) << APP_MEASUREMENT_SOURCE_ADXL345_FEATURE;
  }
  return source->sensor_monitor_schema_revision ==
         APP_SENSOR_MONITOR_SCHEMA_REVISION;
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

bool app_rtos_get_transport_snapshot(app_rtos_transport_snapshot_t *snapshot)
{
  if ((snapshot == NULL) || (app_rtos_event_queue == NULL))
  {
    return false;
  }

  snapshot->current_pending =
      (uint32_t)uxQueueMessagesWaiting(app_rtos_event_queue);
  snapshot->depth = APP_TRANSPORT_EVENT_QUEUE_DEPTH;
  taskENTER_CRITICAL();
  snapshot->counters = app_rtos_transport_counters;
  snapshot->maximum_pending = app_rtos_event_queue_maximum_pending;
  taskEXIT_CRITICAL();
  return true;
}

bool app_rtos_get_reset_reason(app_reset_decoded_t *decoded)
{
  if (decoded == NULL)
  {
    return false;
  }
  *decoded = app_rtos_reset_reason;
  return true;
}

bool app_rtos_get_reset_record(app_reset_record_t *record)
{
  if (record == NULL)
  {
    return false;
  }
  taskENTER_CRITICAL();
  const bool valid = app_reset_record_is_valid(&app_rtos_reset_record);
  if (valid)
  {
    *record = app_rtos_reset_record;
  }
  taskEXIT_CRITICAL();
  return valid;
}

uint32_t app_rtos_fault_code(void)
{
  return app_rtos_current_fault_code;
}

_Noreturn void app_rtos_fail_stop(uint32_t fault_code)
{
  app_rtos_current_fault_code = fault_code;
  (void)app_reset_record_note_fault(&app_rtos_reset_record, fault_code);
  __disable_irq();
  __DSB();
  __ISB();
  for (;;)
  {
    __WFI();
  }
}
