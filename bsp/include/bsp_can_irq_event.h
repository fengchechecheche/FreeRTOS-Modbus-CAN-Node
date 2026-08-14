#ifndef BSP_CAN_IRQ_EVENT_H
#define BSP_CAN_IRQ_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "p5_can_contract.h"

#define BSP_CAN_FILTER_BANK_COUNT UINT32_C(2)
#define BSP_CAN_FILTER_ENTRIES_PER_BANK UINT32_C(4)
#define BSP_CAN_RX_RING_CAPACITY UINT32_C(4)

#define BSP_CAN_IRQ_EVENT_RX_READY UINT32_C(0x00000001)
#define BSP_CAN_IRQ_EVENT_TX_COMPLETE UINT32_C(0x00000002)
#define BSP_CAN_IRQ_EVENT_TX_ABORT UINT32_C(0x00000004)
#define BSP_CAN_IRQ_EVENT_ERROR UINT32_C(0x00000008)
#define BSP_CAN_IRQ_EVENT_MASK                                                 \
  (BSP_CAN_IRQ_EVENT_RX_READY | BSP_CAN_IRQ_EVENT_TX_COMPLETE |                \
   BSP_CAN_IRQ_EVENT_TX_ABORT | BSP_CAN_IRQ_EVENT_ERROR)

#define BSP_CAN_IDE_STANDARD UINT32_C(0)
#define BSP_CAN_RTR_DATA UINT32_C(0)

typedef struct {
  uint16_t entries[BSP_CAN_FILTER_ENTRIES_PER_BANK];
} bsp_can_filter_bank_t;

typedef struct {
  bsp_can_filter_bank_t banks[BSP_CAN_FILTER_BANK_COUNT];
} bsp_can_filter_plan_t;

typedef struct {
  uint32_t standard_id;
  uint32_t ide;
  uint32_t rtr;
  uint32_t dlc;
  uint8_t data[P5_CAN_DLC];
} bsp_can_rx_frame_t;

typedef struct {
  uint32_t published_events;
  uint32_t coalesced_events;
  uint32_t rx_accepted;
  uint32_t rx_dropped;
  uint32_t invalid_headers;
  uint32_t tx_completed;
  uint32_t tx_aborted;
  uint32_t error_callbacks;
  uint32_t deferred_notifications;
  uint32_t snapshots_taken;
} bsp_can_irq_event_counters_t;

typedef struct {
  volatile uint32_t pending_mask;
  bsp_can_rx_frame_t rx_ring[BSP_CAN_RX_RING_CAPACITY];
  volatile uint8_t rx_read_index;
  volatile uint8_t rx_write_index;
  volatile uint8_t rx_count;
  volatile uint8_t tx_complete_mask;
  volatile uint8_t tx_abort_mask;
  volatile uint32_t latest_hal_error;
  volatile uint32_t latest_controller_state;
  bsp_can_irq_event_counters_t counters;
} bsp_can_irq_mailbox_t;

typedef struct {
  uint32_t event_mask;
  uint8_t rx_pending;
  uint8_t tx_complete_mask;
  uint8_t tx_abort_mask;
  uint32_t latest_hal_error;
  uint32_t latest_controller_state;
} bsp_can_irq_event_snapshot_t;

bool bsp_can_filter_encode_standard_id(uint16_t standard_id, uint16_t *encoded);
void bsp_can_filter_plan_build(bsp_can_filter_plan_t *plan);
bool bsp_can_header_is_accepted(uint32_t standard_id, uint32_t ide,
                                uint32_t rtr, uint32_t dlc);

void bsp_can_irq_mailbox_initialize(bsp_can_irq_mailbox_t *mailbox);
uint32_t bsp_can_irq_mailbox_publish_rx(bsp_can_irq_mailbox_t *mailbox,
                                        const bsp_can_rx_frame_t *frame);
uint32_t bsp_can_irq_mailbox_publish_tx_complete(bsp_can_irq_mailbox_t *mailbox,
                                                 uint8_t mailbox_index);
uint32_t bsp_can_irq_mailbox_publish_tx_abort(bsp_can_irq_mailbox_t *mailbox,
                                              uint8_t mailbox_index);
uint32_t bsp_can_irq_mailbox_publish_error(bsp_can_irq_mailbox_t *mailbox,
                                           uint32_t hal_error,
                                           uint32_t controller_state);
bool bsp_can_irq_mailbox_take(bsp_can_irq_mailbox_t *mailbox,
                              bsp_can_irq_event_snapshot_t *snapshot);
bool bsp_can_irq_mailbox_pop_rx(bsp_can_irq_mailbox_t *mailbox,
                                bsp_can_rx_frame_t *frame);
void bsp_can_irq_mailbox_note_deferred(bsp_can_irq_mailbox_t *mailbox);
bsp_can_irq_event_counters_t
bsp_can_irq_mailbox_counters(const bsp_can_irq_mailbox_t *mailbox);

#endif
