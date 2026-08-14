#ifndef BSP_CAN_H
#define BSP_CAN_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp_can_irq_event.h"
#include "stm32f4xx_hal.h"

typedef void (*bsp_can_irq_notifier_t)(uint32_t event_mask);

typedef enum {
  BSP_CAN_SEND_OK = 0,
  BSP_CAN_SEND_BUSY,
  BSP_CAN_SEND_ERROR
} bsp_can_send_result_t;

CAN_HandleTypeDef *bsp_can_handle(void);
bool bsp_can_configure_filters(void);
bool bsp_can_start(void);
bool bsp_can_stop(void);
bsp_can_send_result_t bsp_can_send(const p5_can_frame_t *frame);
uint32_t bsp_can_tx_free_level(void);
void bsp_can_irq_initialize(void);
void bsp_can_register_irq_notifier(bsp_can_irq_notifier_t notifier);
bool bsp_can_take_irq_snapshot(bsp_can_irq_event_snapshot_t *snapshot);
bool bsp_can_take_received(bsp_can_rx_frame_t *frame);
bsp_can_irq_event_counters_t bsp_can_irq_counters(void);

#endif
