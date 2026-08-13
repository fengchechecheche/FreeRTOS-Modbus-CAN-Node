#ifndef BSP_RS485_H
#define BSP_RS485_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp_rs485_state.h"
#include "stm32f4xx_hal.h"

typedef struct
{
  bsp_rs485_link_state_t state;
  bsp_rs485_error_t last_error;
  bsp_rs485_state_counters_t state_counters;
  uint32_t rx_completed;
  uint32_t rx_dropped;
} bsp_rs485_diagnostics_t;

UART_HandleTypeDef *bsp_rs485_uart_handle(void);
void bsp_rs485_set_transmit(bool enabled);
bsp_rs485_result_t bsp_rs485_initialize(void);
bsp_rs485_result_t bsp_rs485_send(const uint8_t *data, size_t length);
bsp_rs485_result_t bsp_rs485_poll(void);
bool bsp_rs485_is_busy(void);
bool bsp_rs485_take_received(uint8_t *destination,
                             size_t capacity,
                             size_t *received_length);
bsp_rs485_diagnostics_t bsp_rs485_get_diagnostics(void);

#endif
