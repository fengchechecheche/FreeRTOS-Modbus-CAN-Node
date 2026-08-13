#ifndef BSP_RS485_H
#define BSP_RS485_H

#include <stdbool.h>

#include "stm32f4xx_hal.h"

UART_HandleTypeDef *bsp_rs485_uart_handle(void);
void bsp_rs485_set_transmit(bool enabled);

#endif
