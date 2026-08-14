#ifndef BSP_RS485_IRQ_EVENT_H
#define BSP_RS485_IRQ_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#define BSP_RS485_IRQ_EVENT_RX_FRAME UINT32_C(0x00000001)
#define BSP_RS485_IRQ_EVENT_RX_HALF UINT32_C(0x00000002)
#define BSP_RS485_IRQ_EVENT_TX_COMPLETE UINT32_C(0x00000004)
#define BSP_RS485_IRQ_EVENT_UART_ERROR UINT32_C(0x00000008)
#define BSP_RS485_IRQ_EVENT_MASK                                           \
  (BSP_RS485_IRQ_EVENT_RX_FRAME | BSP_RS485_IRQ_EVENT_RX_HALF |           \
   BSP_RS485_IRQ_EVENT_TX_COMPLETE | BSP_RS485_IRQ_EVENT_UART_ERROR)

#define BSP_RS485_IRQ_ERROR_INVALID_RX_LENGTH UINT32_C(0x80000000)
#define BSP_RS485_IRQ_ERROR_INVALID_RX_KIND UINT32_C(0x40000000)

typedef enum
{
  BSP_RS485_RX_EVENT_NONE = 0,
  BSP_RS485_RX_EVENT_IDLE,
  BSP_RS485_RX_EVENT_DMA_COMPLETE
} bsp_rs485_rx_event_kind_t;

typedef struct
{
  uint32_t published_events;
  uint32_t coalesced_events;
  uint32_t invalid_rx_lengths;
  uint32_t invalid_rx_kinds;
  uint32_t conflict_snapshots;
  uint32_t deferred_notifications;
  uint32_t snapshots_taken;
} bsp_rs485_irq_event_counters_t;

typedef struct
{
  volatile uint32_t pending_mask;
  volatile uint16_t rx_length;
  volatile bsp_rs485_rx_event_kind_t rx_kind;
  volatile uint32_t rx_captured_cycles;
  volatile uint32_t uart_error;
  bsp_rs485_irq_event_counters_t counters;
} bsp_rs485_irq_mailbox_t;

typedef struct
{
  uint32_t event_mask;
  uint16_t rx_length;
  bsp_rs485_rx_event_kind_t rx_kind;
  uint32_t rx_captured_cycles;
  uint32_t uart_error;
} bsp_rs485_irq_event_snapshot_t;

typedef struct
{
  uint32_t sample_count;
  uint32_t minimum_cycles;
  uint32_t maximum_cycles;
  uint32_t last_cycles;
  bool measured;
} bsp_rs485_irq_latency_summary_t;

void bsp_rs485_irq_mailbox_initialize(bsp_rs485_irq_mailbox_t *mailbox);
uint32_t bsp_rs485_irq_mailbox_publish(bsp_rs485_irq_mailbox_t *mailbox,
                                      uint32_t event_mask,
                                      uint16_t rx_length,
                                      uint32_t uart_error);
uint32_t bsp_rs485_irq_mailbox_publish_rx(
    bsp_rs485_irq_mailbox_t *mailbox,
    bsp_rs485_rx_event_kind_t kind,
    uint16_t rx_length,
    uint32_t captured_cycles);
bool bsp_rs485_irq_mailbox_take(bsp_rs485_irq_mailbox_t *mailbox,
                               bsp_rs485_irq_event_snapshot_t *snapshot);
void bsp_rs485_irq_mailbox_note_deferred(
    bsp_rs485_irq_mailbox_t *mailbox);
bsp_rs485_irq_event_counters_t bsp_rs485_irq_mailbox_counters(
    const bsp_rs485_irq_mailbox_t *mailbox);

void bsp_rs485_irq_latency_reset(bsp_rs485_irq_latency_summary_t *summary);
void bsp_rs485_irq_latency_record(bsp_rs485_irq_latency_summary_t *summary,
                                  uint32_t isr_cycles,
                                  uint32_t task_cycles);

#endif
