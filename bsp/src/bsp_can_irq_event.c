#include "bsp_can_irq_event.h"

#include <stddef.h>
#include <string.h>

static const uint16_t bsp_can_allowed_ids[] = {
    P5_CAN_ID_STATUS_EVENT,      P5_CAN_ID_HEARTBEAT,
    P5_CAN_ID_HEALTH_SUMMARY,    P5_CAN_ID_CLIMATE_PRIMARY,
    P5_CAN_ID_CLIMATE_SECONDARY, P5_CAN_ID_ILLUMINANCE,
    P5_CAN_ID_VIBRATION_SUMMARY,
    P5_CAN_ID_DIAGNOSTIC_REQUEST,
};

static uint32_t bsp_can_saturating_increment(uint32_t value) {
  return (value == UINT32_MAX) ? UINT32_MAX : (value + 1U);
}

static uint32_t bsp_can_irq_publish_event(bsp_can_irq_mailbox_t *mailbox,
                                          uint32_t event_mask) {
  if ((mailbox == NULL) || ((event_mask & BSP_CAN_IRQ_EVENT_MASK) == 0U)) {
    return 0U;
  }

  const uint32_t accepted = event_mask & BSP_CAN_IRQ_EVENT_MASK;
  if ((mailbox->pending_mask & accepted) != 0U) {
    mailbox->counters.coalesced_events =
        bsp_can_saturating_increment(mailbox->counters.coalesced_events);
  }
  mailbox->counters.published_events =
      bsp_can_saturating_increment(mailbox->counters.published_events);
  mailbox->pending_mask |= accepted;
  return accepted;
}

bool bsp_can_filter_encode_standard_id(uint16_t standard_id,
                                       uint16_t *encoded) {
  if ((encoded == NULL) || (standard_id > UINT16_C(0x07ff))) {
    return false;
  }
  *encoded = (uint16_t)(standard_id << 5U);
  return true;
}

void bsp_can_filter_plan_build(bsp_can_filter_plan_t *plan) {
  if (plan == NULL) {
    return;
  }

  for (uint32_t index = 0U;
       index < (BSP_CAN_FILTER_BANK_COUNT * BSP_CAN_FILTER_ENTRIES_PER_BANK);
       ++index) {
    const uint32_t id_count =
        sizeof(bsp_can_allowed_ids) / sizeof(bsp_can_allowed_ids[0]);
    const uint32_t id_index = (index < id_count) ? index : (id_count - 1U);
    uint16_t encoded = 0U;
    (void)bsp_can_filter_encode_standard_id(bsp_can_allowed_ids[id_index],
                                            &encoded);
    plan->banks[index / BSP_CAN_FILTER_ENTRIES_PER_BANK]
        .entries[index % BSP_CAN_FILTER_ENTRIES_PER_BANK] = encoded;
  }
}

bool bsp_can_header_is_accepted(uint32_t standard_id, uint32_t ide,
                                uint32_t rtr, uint32_t dlc) {
  if ((ide != BSP_CAN_IDE_STANDARD) || (rtr != BSP_CAN_RTR_DATA) ||
      (dlc != P5_CAN_DLC)) {
    return false;
  }

  for (uint32_t index = 0U;
       index < (sizeof(bsp_can_allowed_ids) / sizeof(bsp_can_allowed_ids[0]));
       ++index) {
    if (standard_id == bsp_can_allowed_ids[index]) {
      return true;
    }
  }
  return false;
}

void bsp_can_irq_mailbox_initialize(bsp_can_irq_mailbox_t *mailbox) {
  if (mailbox != NULL) {
    (void)memset(mailbox, 0, sizeof(*mailbox));
  }
}

uint32_t bsp_can_irq_mailbox_publish_rx(bsp_can_irq_mailbox_t *mailbox,
                                        const bsp_can_rx_frame_t *frame) {
  if ((mailbox == NULL) || (frame == NULL)) {
    return 0U;
  }
  if (!bsp_can_header_is_accepted(frame->standard_id, frame->ide, frame->rtr,
                                  frame->dlc)) {
    mailbox->counters.invalid_headers =
        bsp_can_saturating_increment(mailbox->counters.invalid_headers);
    return 0U;
  }
  if (mailbox->rx_count >= BSP_CAN_RX_RING_CAPACITY) {
    mailbox->counters.rx_dropped =
        bsp_can_saturating_increment(mailbox->counters.rx_dropped);
    return 0U;
  }

  mailbox->rx_ring[mailbox->rx_write_index] = *frame;
  mailbox->rx_write_index =
      (uint8_t)((mailbox->rx_write_index + 1U) % BSP_CAN_RX_RING_CAPACITY);
  ++mailbox->rx_count;
  mailbox->counters.rx_accepted =
      bsp_can_saturating_increment(mailbox->counters.rx_accepted);
  return bsp_can_irq_publish_event(mailbox, BSP_CAN_IRQ_EVENT_RX_READY);
}

uint32_t bsp_can_irq_mailbox_publish_tx_complete(bsp_can_irq_mailbox_t *mailbox,
                                                 uint8_t mailbox_index) {
  if ((mailbox == NULL) || (mailbox_index >= 3U)) {
    return 0U;
  }
  mailbox->tx_complete_mask |= (uint8_t)(1U << mailbox_index);
  mailbox->counters.tx_completed =
      bsp_can_saturating_increment(mailbox->counters.tx_completed);
  return bsp_can_irq_publish_event(mailbox, BSP_CAN_IRQ_EVENT_TX_COMPLETE);
}

uint32_t bsp_can_irq_mailbox_publish_tx_abort(bsp_can_irq_mailbox_t *mailbox,
                                              uint8_t mailbox_index) {
  if ((mailbox == NULL) || (mailbox_index >= 3U)) {
    return 0U;
  }
  mailbox->tx_abort_mask |= (uint8_t)(1U << mailbox_index);
  mailbox->counters.tx_aborted =
      bsp_can_saturating_increment(mailbox->counters.tx_aborted);
  return bsp_can_irq_publish_event(mailbox, BSP_CAN_IRQ_EVENT_TX_ABORT);
}

uint32_t bsp_can_irq_mailbox_publish_error(bsp_can_irq_mailbox_t *mailbox,
                                           uint32_t hal_error,
                                           uint32_t controller_state) {
  if (mailbox == NULL) {
    return 0U;
  }
  mailbox->latest_hal_error = hal_error;
  mailbox->latest_controller_state = controller_state;
  mailbox->counters.error_callbacks =
      bsp_can_saturating_increment(mailbox->counters.error_callbacks);
  return bsp_can_irq_publish_event(mailbox, BSP_CAN_IRQ_EVENT_ERROR);
}

bool bsp_can_irq_mailbox_take(bsp_can_irq_mailbox_t *mailbox,
                              bsp_can_irq_event_snapshot_t *snapshot) {
  if ((mailbox == NULL) || (snapshot == NULL) ||
      (mailbox->pending_mask == 0U)) {
    return false;
  }

  snapshot->event_mask = mailbox->pending_mask;
  snapshot->rx_pending = mailbox->rx_count;
  snapshot->tx_complete_mask = mailbox->tx_complete_mask;
  snapshot->tx_abort_mask = mailbox->tx_abort_mask;
  snapshot->latest_hal_error = mailbox->latest_hal_error;
  snapshot->latest_controller_state = mailbox->latest_controller_state;

  mailbox->pending_mask =
      (mailbox->rx_count > 0U) ? BSP_CAN_IRQ_EVENT_RX_READY : 0U;
  mailbox->tx_complete_mask = 0U;
  mailbox->tx_abort_mask = 0U;
  mailbox->latest_hal_error = 0U;
  mailbox->latest_controller_state = 0U;
  mailbox->counters.snapshots_taken =
      bsp_can_saturating_increment(mailbox->counters.snapshots_taken);
  return true;
}

bool bsp_can_irq_mailbox_pop_rx(bsp_can_irq_mailbox_t *mailbox,
                                bsp_can_rx_frame_t *frame) {
  if ((mailbox == NULL) || (frame == NULL) || (mailbox->rx_count == 0U)) {
    return false;
  }

  *frame = mailbox->rx_ring[mailbox->rx_read_index];
  mailbox->rx_read_index =
      (uint8_t)((mailbox->rx_read_index + 1U) % BSP_CAN_RX_RING_CAPACITY);
  --mailbox->rx_count;
  if (mailbox->rx_count == 0U) {
    mailbox->pending_mask &= ~BSP_CAN_IRQ_EVENT_RX_READY;
  }
  return true;
}

void bsp_can_irq_mailbox_note_deferred(bsp_can_irq_mailbox_t *mailbox) {
  if (mailbox != NULL) {
    mailbox->counters.deferred_notifications =
        bsp_can_saturating_increment(mailbox->counters.deferred_notifications);
  }
}

bsp_can_irq_event_counters_t
bsp_can_irq_mailbox_counters(const bsp_can_irq_mailbox_t *mailbox) {
  const bsp_can_irq_event_counters_t empty = {0U};
  return (mailbox == NULL) ? empty : mailbox->counters;
}
