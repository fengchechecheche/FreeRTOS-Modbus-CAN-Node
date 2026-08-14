#include "bsp_rs485_irq_event.h"

#include <stddef.h>

#include "bsp_rs485_state.h"

static uint32_t bsp_rs485_irq_saturating_increment(uint32_t value)
{
  return (value == UINT32_MAX) ? UINT32_MAX : (value + 1U);
}

static uint32_t bsp_rs485_irq_count_bits(uint32_t value)
{
  uint32_t count = 0U;
  while (value != 0U)
  {
    count = bsp_rs485_irq_saturating_increment(count);
    value &= value - 1U;
  }
  return count;
}

static uint32_t bsp_rs485_irq_saturating_add(uint32_t value,
                                             uint32_t increment)
{
  if (increment > (UINT32_MAX - value))
  {
    return UINT32_MAX;
  }
  return value + increment;
}

void bsp_rs485_irq_mailbox_initialize(bsp_rs485_irq_mailbox_t *mailbox)
{
  if (mailbox == NULL)
  {
    return;
  }

  mailbox->pending_mask = 0U;
  mailbox->rx_length = 0U;
  mailbox->rx_kind = BSP_RS485_RX_EVENT_NONE;
  mailbox->rx_captured_cycles = 0U;
  mailbox->uart_error = 0U;
  mailbox->counters.published_events = 0U;
  mailbox->counters.coalesced_events = 0U;
  mailbox->counters.invalid_rx_lengths = 0U;
  mailbox->counters.invalid_rx_kinds = 0U;
  mailbox->counters.conflict_snapshots = 0U;
  mailbox->counters.deferred_notifications = 0U;
  mailbox->counters.snapshots_taken = 0U;
}

uint32_t bsp_rs485_irq_mailbox_publish(bsp_rs485_irq_mailbox_t *mailbox,
                                      uint32_t event_mask,
                                      uint16_t rx_length,
                                      uint32_t uart_error)
{
  if (mailbox == NULL)
  {
    return 0U;
  }

  uint32_t accepted = event_mask & BSP_RS485_IRQ_EVENT_MASK;
  if (((accepted & BSP_RS485_IRQ_EVENT_RX_FRAME) != 0U) &&
      ((rx_length == 0U) ||
       (rx_length > BSP_RS485_RX_DMA_CHUNK_SIZE)))
  {
    accepted &= ~BSP_RS485_IRQ_EVENT_RX_FRAME;
    accepted |= BSP_RS485_IRQ_EVENT_UART_ERROR;
    uart_error |= BSP_RS485_IRQ_ERROR_INVALID_RX_LENGTH;
    mailbox->counters.invalid_rx_lengths = bsp_rs485_irq_saturating_increment(
        mailbox->counters.invalid_rx_lengths);
  }

  if (accepted == 0U)
  {
    return 0U;
  }

  const uint32_t pending = mailbox->pending_mask;
  mailbox->counters.published_events = bsp_rs485_irq_saturating_add(
      mailbox->counters.published_events, bsp_rs485_irq_count_bits(accepted));
  mailbox->counters.coalesced_events = bsp_rs485_irq_saturating_add(
      mailbox->counters.coalesced_events,
      bsp_rs485_irq_count_bits(pending & accepted));

  if (((accepted & BSP_RS485_IRQ_EVENT_RX_FRAME) != 0U) &&
      ((pending & BSP_RS485_IRQ_EVENT_RX_FRAME) == 0U))
  {
    mailbox->rx_length = rx_length;
    mailbox->rx_kind = BSP_RS485_RX_EVENT_DMA_COMPLETE;
    mailbox->rx_captured_cycles = 0U;
  }
  if ((accepted & BSP_RS485_IRQ_EVENT_UART_ERROR) != 0U)
  {
    mailbox->uart_error |= uart_error;
  }

  mailbox->pending_mask = pending | accepted;
  return accepted;
}

uint32_t bsp_rs485_irq_mailbox_publish_rx(
    bsp_rs485_irq_mailbox_t *mailbox,
    bsp_rs485_rx_event_kind_t kind,
    uint16_t rx_length,
    uint32_t captured_cycles)
{
  if (mailbox == NULL)
  {
    return 0U;
  }
  if ((kind != BSP_RS485_RX_EVENT_IDLE) &&
      (kind != BSP_RS485_RX_EVENT_DMA_COMPLETE))
  {
    mailbox->counters.invalid_rx_kinds = bsp_rs485_irq_saturating_increment(
        mailbox->counters.invalid_rx_kinds);
    return bsp_rs485_irq_mailbox_publish(
        mailbox,
        BSP_RS485_IRQ_EVENT_UART_ERROR,
        0U,
        BSP_RS485_IRQ_ERROR_INVALID_RX_KIND);
  }

  const bool rx_was_pending =
      (mailbox->pending_mask & BSP_RS485_IRQ_EVENT_RX_FRAME) != 0U;
  const uint32_t accepted = bsp_rs485_irq_mailbox_publish(
      mailbox, BSP_RS485_IRQ_EVENT_RX_FRAME, rx_length, 0U);
  if (!rx_was_pending &&
      ((accepted & BSP_RS485_IRQ_EVENT_RX_FRAME) != 0U))
  {
    mailbox->rx_kind = kind;
    mailbox->rx_captured_cycles = captured_cycles;
  }
  return accepted;
}

bool bsp_rs485_irq_mailbox_take(bsp_rs485_irq_mailbox_t *mailbox,
                               bsp_rs485_irq_event_snapshot_t *snapshot)
{
  if ((mailbox == NULL) || (snapshot == NULL) ||
      (mailbox->pending_mask == 0U))
  {
    return false;
  }

  snapshot->event_mask = mailbox->pending_mask;
  snapshot->rx_length = mailbox->rx_length;
  snapshot->rx_kind = mailbox->rx_kind;
  snapshot->rx_captured_cycles = mailbox->rx_captured_cycles;
  snapshot->uart_error = mailbox->uart_error;

  mailbox->pending_mask = 0U;
  mailbox->rx_length = 0U;
  mailbox->rx_kind = BSP_RS485_RX_EVENT_NONE;
  mailbox->rx_captured_cycles = 0U;
  mailbox->uart_error = 0U;
  mailbox->counters.snapshots_taken = bsp_rs485_irq_saturating_increment(
      mailbox->counters.snapshots_taken);

  const bool error_conflict =
      ((snapshot->event_mask & BSP_RS485_IRQ_EVENT_UART_ERROR) != 0U) &&
      ((snapshot->event_mask &
        (BSP_RS485_IRQ_EVENT_RX_FRAME |
         BSP_RS485_IRQ_EVENT_TX_COMPLETE)) != 0U);
  const bool direction_conflict =
      (snapshot->event_mask &
       (BSP_RS485_IRQ_EVENT_RX_FRAME |
        BSP_RS485_IRQ_EVENT_TX_COMPLETE)) ==
      (BSP_RS485_IRQ_EVENT_RX_FRAME |
       BSP_RS485_IRQ_EVENT_TX_COMPLETE);
  if (error_conflict || direction_conflict)
  {
    mailbox->counters.conflict_snapshots = bsp_rs485_irq_saturating_increment(
        mailbox->counters.conflict_snapshots);
  }
  return true;
}

void bsp_rs485_irq_mailbox_note_deferred(
    bsp_rs485_irq_mailbox_t *mailbox)
{
  if (mailbox != NULL)
  {
    mailbox->counters.deferred_notifications =
        bsp_rs485_irq_saturating_increment(
            mailbox->counters.deferred_notifications);
  }
}

bsp_rs485_irq_event_counters_t bsp_rs485_irq_mailbox_counters(
    const bsp_rs485_irq_mailbox_t *mailbox)
{
  const bsp_rs485_irq_event_counters_t empty =
      {0U, 0U, 0U, 0U, 0U, 0U, 0U};
  return (mailbox == NULL) ? empty : mailbox->counters;
}

void bsp_rs485_irq_latency_reset(bsp_rs485_irq_latency_summary_t *summary)
{
  if (summary == NULL)
  {
    return;
  }
  summary->sample_count = 0U;
  summary->minimum_cycles = 0U;
  summary->maximum_cycles = 0U;
  summary->last_cycles = 0U;
  summary->measured = false;
}

void bsp_rs485_irq_latency_record(bsp_rs485_irq_latency_summary_t *summary,
                                  uint32_t isr_cycles,
                                  uint32_t task_cycles)
{
  if (summary == NULL)
  {
    return;
  }

  const uint32_t elapsed_cycles = task_cycles - isr_cycles;
  if (!summary->measured)
  {
    summary->minimum_cycles = elapsed_cycles;
    summary->maximum_cycles = elapsed_cycles;
    summary->measured = true;
  }
  else
  {
    if (elapsed_cycles < summary->minimum_cycles)
    {
      summary->minimum_cycles = elapsed_cycles;
    }
    if (elapsed_cycles > summary->maximum_cycles)
    {
      summary->maximum_cycles = elapsed_cycles;
    }
  }
  summary->last_cycles = elapsed_cycles;
  summary->sample_count =
      bsp_rs485_irq_saturating_increment(summary->sample_count);
}
