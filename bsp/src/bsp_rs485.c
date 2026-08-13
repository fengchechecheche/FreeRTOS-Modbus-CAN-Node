#include "bsp_rs485.h"

#include "main.h"
#include "usart.h"

UART_HandleTypeDef *bsp_rs485_uart_handle(void)
{
  return &huart1;
}

void bsp_rs485_set_transmit(bool enabled)
{
  HAL_GPIO_WritePin(RS485_DE_GPIO_Port,
                    RS485_DE_Pin,
                    enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
