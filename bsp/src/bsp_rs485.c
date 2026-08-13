#include "bsp_rs485.h"

#include <string.h>

#include "bsp_clock.h"
#include "main.h"
#include "usart.h"

static bsp_rs485_state_controller_t bsp_rs485_controller;
static uint8_t bsp_rs485_rx_dma_buffer[BSP_RS485_MAX_FRAME_SIZE];
static uint8_t bsp_rs485_rx_frame[BSP_RS485_MAX_FRAME_SIZE];
static volatile size_t bsp_rs485_rx_length;
static volatile bool bsp_rs485_rx_ready;
static volatile uint32_t bsp_rs485_rx_completed;
static volatile uint32_t bsp_rs485_rx_dropped;

static void bsp_rs485_adapter_set_transmit(void *context, bool enabled)
{
  (void)context;
  bsp_rs485_set_transmit(enabled);
}

static bool bsp_rs485_adapter_start_tx(void *context,
                                       const uint8_t *data,
                                       size_t length)
{
  (void)context;
  return HAL_UART_Transmit_DMA(&huart1, data, (uint16_t)length) == HAL_OK;
}

static void bsp_rs485_adapter_abort_tx(void *context)
{
  (void)context;
  (void)HAL_UART_AbortTransmit(&huart1);
}

static bool bsp_rs485_adapter_arm_rx(void *context)
{
  (void)context;
  if (huart1.RxState == HAL_UART_STATE_BUSY_RX)
  {
    return true;
  }

  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1,
                                   bsp_rs485_rx_dma_buffer,
                                   BSP_RS485_MAX_FRAME_SIZE) != HAL_OK)
  {
    return false;
  }

  if (huart1.hdmarx != NULL)
  {
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
  }
  return true;
}

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

bsp_rs485_result_t bsp_rs485_initialize(void)
{
  bsp_rs485_rx_length = 0U;
  bsp_rs485_rx_ready = false;
  bsp_rs485_rx_completed = 0U;
  bsp_rs485_rx_dropped = 0U;

  const bsp_rs485_state_ops_t ops = {
      .context = NULL,
      .set_transmit = bsp_rs485_adapter_set_transmit,
      .start_tx_dma = bsp_rs485_adapter_start_tx,
      .abort_tx = bsp_rs485_adapter_abort_tx,
      .arm_rx_dma = bsp_rs485_adapter_arm_rx,
  };
  return bsp_rs485_state_initialize(&bsp_rs485_controller, &ops);
}

bsp_rs485_result_t bsp_rs485_send(const uint8_t *data, size_t length)
{
  return bsp_rs485_state_send(&bsp_rs485_controller,
                              data,
                              length,
                              bsp_clock_tick_ms());
}

bsp_rs485_result_t bsp_rs485_poll(void)
{
  return bsp_rs485_state_poll(&bsp_rs485_controller,
                              bsp_clock_tick_ms());
}

bool bsp_rs485_is_busy(void)
{
  return bsp_rs485_controller.state != BSP_RS485_LINK_IDLE_RX;
}

bool bsp_rs485_take_received(uint8_t *destination,
                             size_t capacity,
                             size_t *received_length)
{
  if ((destination == NULL) || (received_length == NULL))
  {
    return false;
  }

  const uint32_t previous_primask = __get_PRIMASK();
  __disable_irq();
  if (!bsp_rs485_rx_ready || (capacity < bsp_rs485_rx_length))
  {
    if (previous_primask == 0U)
    {
      __enable_irq();
    }
    return false;
  }

  const size_t length = bsp_rs485_rx_length;
  (void)memcpy(destination, bsp_rs485_rx_frame, length);
  bsp_rs485_rx_ready = false;
  bsp_rs485_rx_length = 0U;
  if (previous_primask == 0U)
  {
    __enable_irq();
  }

  *received_length = length;
  return true;
}

bsp_rs485_diagnostics_t bsp_rs485_get_diagnostics(void)
{
  bsp_rs485_diagnostics_t diagnostics;
  const uint32_t previous_primask = __get_PRIMASK();
  __disable_irq();
  diagnostics.state = bsp_rs485_controller.state;
  diagnostics.last_error = bsp_rs485_controller.last_error;
  diagnostics.state_counters = bsp_rs485_controller.counters;
  diagnostics.rx_completed = bsp_rs485_rx_completed;
  diagnostics.rx_dropped = bsp_rs485_rx_dropped;
  if (previous_primask == 0U)
  {
    __enable_irq();
  }
  return diagnostics;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    bsp_rs485_state_on_tx_complete(&bsp_rs485_controller);
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (huart != &huart1)
  {
    return;
  }

  if ((size > 0U) && (size <= BSP_RS485_MAX_FRAME_SIZE))
  {
    if (!bsp_rs485_rx_ready)
    {
      (void)memcpy(bsp_rs485_rx_frame, bsp_rs485_rx_dma_buffer, size);
      bsp_rs485_rx_length = size;
      bsp_rs485_rx_ready = true;
      ++bsp_rs485_rx_completed;
    }
    else
    {
      ++bsp_rs485_rx_dropped;
    }
  }

  if (!bsp_rs485_adapter_arm_rx(NULL))
  {
    bsp_rs485_state_on_io_error(&bsp_rs485_controller);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    bsp_rs485_state_on_io_error(&bsp_rs485_controller);
  }
}
