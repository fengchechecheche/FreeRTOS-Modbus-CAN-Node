#include "app_task_model.h"
#include "bsp_rs485_irq_event.h"

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

static int test_empty_and_early_event(void)
{
  bsp_rs485_irq_mailbox_t mailbox;
  bsp_rs485_irq_event_snapshot_t snapshot;
  bsp_rs485_irq_mailbox_initialize(&mailbox);
  CHECK(!bsp_rs485_irq_mailbox_take(&mailbox, &snapshot));

  CHECK(bsp_rs485_irq_mailbox_publish(
            &mailbox, BSP_RS485_IRQ_EVENT_RX_FRAME, 7U, 0U) ==
        BSP_RS485_IRQ_EVENT_RX_FRAME);
  bsp_rs485_irq_mailbox_note_deferred(&mailbox);
  CHECK(bsp_rs485_irq_mailbox_take(&mailbox, &snapshot));
  CHECK(snapshot.event_mask == BSP_RS485_IRQ_EVENT_RX_FRAME);
  CHECK(snapshot.rx_length == 7U);
  const bsp_rs485_irq_event_counters_t counters =
      bsp_rs485_irq_mailbox_counters(&mailbox);
  CHECK(counters.deferred_notifications == 1U);
  CHECK(counters.snapshots_taken == 1U);
  return EXIT_SUCCESS;
}

static int test_duplicate_event_is_coalesced(void)
{
  bsp_rs485_irq_mailbox_t mailbox;
  bsp_rs485_irq_event_snapshot_t snapshot;
  bsp_rs485_irq_mailbox_initialize(&mailbox);

  CHECK(bsp_rs485_irq_mailbox_publish(
            &mailbox, BSP_RS485_IRQ_EVENT_RX_FRAME, 8U, 0U) != 0U);
  CHECK(bsp_rs485_irq_mailbox_publish(
            &mailbox, BSP_RS485_IRQ_EVENT_RX_FRAME, 9U, 0U) != 0U);
  CHECK(bsp_rs485_irq_mailbox_take(&mailbox, &snapshot));
  CHECK(snapshot.rx_length == 8U);
  const bsp_rs485_irq_event_counters_t counters =
      bsp_rs485_irq_mailbox_counters(&mailbox);
  CHECK(counters.published_events == 2U);
  CHECK(counters.coalesced_events == 1U);
  return EXIT_SUCCESS;
}

static int test_half_and_invalid_length_are_bounded(void)
{
  bsp_rs485_irq_mailbox_t mailbox;
  bsp_rs485_irq_event_snapshot_t snapshot;
  bsp_rs485_irq_mailbox_initialize(&mailbox);

  CHECK(bsp_rs485_irq_mailbox_publish(
            &mailbox, BSP_RS485_IRQ_EVENT_RX_HALF, 32U, 0U) ==
        BSP_RS485_IRQ_EVENT_RX_HALF);
  CHECK(bsp_rs485_irq_mailbox_publish(
            &mailbox, BSP_RS485_IRQ_EVENT_RX_FRAME, 0U, 0U) ==
        BSP_RS485_IRQ_EVENT_UART_ERROR);
  CHECK(bsp_rs485_irq_mailbox_take(&mailbox, &snapshot));
  CHECK(snapshot.event_mask ==
        (BSP_RS485_IRQ_EVENT_RX_HALF |
         BSP_RS485_IRQ_EVENT_UART_ERROR));
  CHECK((snapshot.uart_error &
         BSP_RS485_IRQ_ERROR_INVALID_RX_LENGTH) != 0U);
  const bsp_rs485_irq_event_counters_t counters =
      bsp_rs485_irq_mailbox_counters(&mailbox);
  CHECK(counters.invalid_rx_lengths == 1U);
  return EXIT_SUCCESS;
}

static int test_conflicting_snapshot_is_diagnostic(void)
{
  bsp_rs485_irq_mailbox_t mailbox;
  bsp_rs485_irq_event_snapshot_t snapshot;
  bsp_rs485_irq_mailbox_initialize(&mailbox);

  CHECK(bsp_rs485_irq_mailbox_publish(
            &mailbox, BSP_RS485_IRQ_EVENT_TX_COMPLETE, 0U, 0U) != 0U);
  CHECK(bsp_rs485_irq_mailbox_publish(
            &mailbox, BSP_RS485_IRQ_EVENT_UART_ERROR, 0U, 4U) != 0U);
  CHECK(bsp_rs485_irq_mailbox_take(&mailbox, &snapshot));
  CHECK(snapshot.event_mask ==
        (BSP_RS485_IRQ_EVENT_TX_COMPLETE |
         BSP_RS485_IRQ_EVENT_UART_ERROR));
  CHECK(snapshot.uart_error == 4U);
  CHECK(bsp_rs485_irq_mailbox_counters(&mailbox).conflict_snapshots == 1U);
  return EXIT_SUCCESS;
}

static int test_latency_summary_and_cycle_wrap(void)
{
  bsp_rs485_irq_latency_summary_t summary;
  bsp_rs485_irq_latency_reset(&summary);
  CHECK(!summary.measured);

  bsp_rs485_irq_latency_record(
      &summary, UINT32_C(0xfffffff0), UINT32_C(0x00000010));
  bsp_rs485_irq_latency_record(&summary, 100U, 105U);
  bsp_rs485_irq_latency_record(&summary, 200U, 264U);
  CHECK(summary.measured);
  CHECK(summary.sample_count == 3U);
  CHECK(summary.minimum_cycles == 5U);
  CHECK(summary.maximum_cycles == 64U);
  CHECK(summary.last_cycles == 64U);
  return EXIT_SUCCESS;
}

static int test_notification_storm_does_not_move_release(void)
{
  const app_task_contract_t *contract =
      app_task_model_contract(APP_TASK_PROTOCOL);
  app_task_runtime_t runtime;
  CHECK(contract != NULL);
  app_task_runtime_initialize(&runtime, 0U);
  CHECK(app_task_runtime_record_cycle(&runtime, contract, 0U, 0U));
  CHECK(runtime.next_release_tick == 5U);

  for (uint32_t event_tick = 1U; event_tick < 5U; ++event_tick)
  {
    CHECK(!app_task_runtime_release_due(&runtime, event_tick));
    CHECK(app_task_runtime_ticks_until_release(&runtime, event_tick) ==
          (5U - event_tick));
  }
  CHECK(app_task_runtime_release_due(&runtime, 5U));
  CHECK(app_task_runtime_ticks_until_release(&runtime, 5U) == 0U);
  CHECK(app_task_runtime_record_cycle(&runtime, contract, 5U, 5U));
  CHECK(runtime.next_release_tick == 10U);

  app_task_runtime_initialize(&runtime, UINT32_C(0xfffffffe));
  CHECK(app_task_runtime_record_cycle(
      &runtime, contract, UINT32_C(0xfffffffe), UINT32_C(0xfffffffe)));
  CHECK(runtime.next_release_tick == 3U);
  CHECK(app_task_runtime_ticks_until_release(&runtime, 0U) == 3U);
  CHECK(app_task_runtime_release_due(&runtime, 3U));
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(test_empty_and_early_event() == EXIT_SUCCESS);
  CHECK(test_duplicate_event_is_coalesced() == EXIT_SUCCESS);
  CHECK(test_half_and_invalid_length_are_bounded() == EXIT_SUCCESS);
  CHECK(test_conflicting_snapshot_is_diagnostic() == EXIT_SUCCESS);
  CHECK(test_latency_summary_and_cycle_wrap() == EXIT_SUCCESS);
  CHECK(test_notification_storm_does_not_move_release() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
