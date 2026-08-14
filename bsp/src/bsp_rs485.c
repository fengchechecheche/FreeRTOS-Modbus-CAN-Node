#include "bsp_rs485.h"

#include <string.h>

#include "bsp_clock.h"
#include "main.h"
#include "usart.h"

static bsp_rs485_state_controller_t bsp_rs485_controller;
static uint8_t bsp_rs485_rx_dma_buffer[BSP_RS485_RX_DMA_CHUNK_SIZE];
static uint8_t bsp_rs485_rx_frame[BSP_RS485_RX_DMA_CHUNK_SIZE];
static volatile size_t bsp_rs485_rx_length;
static volatile bsp_rs485_rx_event_kind_t bsp_rs485_rx_kind;
static volatile uint32_t bsp_rs485_rx_captured_cycles;
static volatile bool bsp_rs485_rx_ready;
static volatile uint32_t bsp_rs485_rx_completed;
static volatile uint32_t bsp_rs485_rx_dropped;
static volatile uint32_t bsp_rs485_last_hal_error;
static bsp_rs485_irq_mailbox_t bsp_rs485_irq_mailbox;
static bsp_rs485_irq_notifier_t bsp_rs485_irq_notifier;

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

static bool bsp_rs485_adapter_stop_rx(void *context)
{
  (void)context;
  if (huart1.RxState != HAL_UART_STATE_BUSY_RX)
  {
    return true;
  }
  return HAL_UART_AbortReceive(&huart1) == HAL_OK;
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
                                   BSP_RS485_RX_DMA_CHUNK_SIZE) != HAL_OK)
  {
    return false;
  }

  if (huart1.hdmarx != NULL)
  {
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
  }
  return true;
}

static void bsp_rs485_set_receive_immediate(void)
{
  RS485_DE_GPIO_Port->BSRR = (uint32_t)RS485_DE_Pin << 16U;
}

static void bsp_rs485_irq_publish_from_isr(uint32_t event_mask,
                                           uint16_t rx_length,
                                           uint32_t uart_error)
{
  const uint32_t accepted = bsp_rs485_irq_mailbox_publish(
      &bsp_rs485_irq_mailbox, event_mask, rx_length, uart_error);
  if (accepted == 0U)
  {
    return;
  }

  const bsp_rs485_irq_notifier_t notifier = bsp_rs485_irq_notifier;
  if (notifier != NULL)
  {
    notifier(accepted);
  }
  else
  {
    bsp_rs485_irq_mailbox_note_deferred(&bsp_rs485_irq_mailbox);
  }
}

static void bsp_rs485_irq_publish_rx_from_isr(
    bsp_rs485_rx_event_kind_t kind,
    uint16_t rx_length,
    uint32_t captured_cycles)
{
  const uint32_t accepted = bsp_rs485_irq_mailbox_publish_rx(
      &bsp_rs485_irq_mailbox, kind, rx_length, captured_cycles);
  if (accepted == 0U)
  {
    return;
  }

  const bsp_rs485_irq_notifier_t notifier = bsp_rs485_irq_notifier;
  if (notifier != NULL)
  {
    notifier(accepted);
  }
  else
  {
    bsp_rs485_irq_mailbox_note_deferred(&bsp_rs485_irq_mailbox);
  }
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
  bsp_rs485_rx_kind = BSP_RS485_RX_EVENT_NONE;
  bsp_rs485_rx_captured_cycles = 0U;
  bsp_rs485_rx_ready = false;
  bsp_rs485_rx_completed = 0U;
  bsp_rs485_rx_dropped = 0U;
  bsp_rs485_last_hal_error = 0U;
  bsp_rs485_irq_notifier = NULL;
  bsp_rs485_irq_mailbox_initialize(&bsp_rs485_irq_mailbox);

  const bsp_rs485_state_ops_t ops = {
      .context = NULL,
      .set_transmit = bsp_rs485_adapter_set_transmit,
      .stop_rx_dma = bsp_rs485_adapter_stop_rx,
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

void bsp_rs485_register_irq_notifier(bsp_rs485_irq_notifier_t notifier)
{
  const uint32_t previous_primask = __get_PRIMASK();
  __disable_irq();
  bsp_rs485_irq_notifier = notifier;
  if (previous_primask == 0U)
  {
    __enable_irq();
  }
}

uint32_t bsp_rs485_service_irq_events(void)
{
  bsp_rs485_irq_event_snapshot_t snapshot;
  const uint32_t previous_primask = __get_PRIMASK();
  __disable_irq();
  const bool available =
      bsp_rs485_irq_mailbox_take(&bsp_rs485_irq_mailbox, &snapshot);
  if (previous_primask == 0U)
  {
    __enable_irq();
  }
  if (!available)
  {
    return 0U;
  }

  const bool direction_conflict =
      (snapshot.event_mask &
       (BSP_RS485_IRQ_EVENT_RX_FRAME |
        BSP_RS485_IRQ_EVENT_TX_COMPLETE)) ==
      (BSP_RS485_IRQ_EVENT_RX_FRAME |
       BSP_RS485_IRQ_EVENT_TX_COMPLETE);
  if (((snapshot.event_mask & BSP_RS485_IRQ_EVENT_UART_ERROR) != 0U) ||
      direction_conflict)
  {
    if ((snapshot.event_mask & BSP_RS485_IRQ_EVENT_RX_FRAME) != 0U)
    {
      ++bsp_rs485_rx_dropped;
    }
    bsp_rs485_last_hal_error = snapshot.uart_error;
    bsp_rs485_state_on_io_error(&bsp_rs485_controller);
    (void)bsp_rs485_state_poll(&bsp_rs485_controller, bsp_clock_tick_ms());
    return snapshot.event_mask;
  }

  if ((snapshot.event_mask & BSP_RS485_IRQ_EVENT_TX_COMPLETE) != 0U)
  {
    bsp_rs485_state_on_tx_complete(&bsp_rs485_controller);
  }

  if ((snapshot.event_mask & BSP_RS485_IRQ_EVENT_RX_FRAME) != 0U)
  {
    if (!bsp_rs485_rx_ready)
    {
      (void)memcpy(bsp_rs485_rx_frame,
                   bsp_rs485_rx_dma_buffer,
                   snapshot.rx_length);
      bsp_rs485_rx_length = snapshot.rx_length;
      bsp_rs485_rx_kind = snapshot.rx_kind;
      bsp_rs485_rx_captured_cycles = snapshot.rx_captured_cycles;
      bsp_rs485_rx_ready = true;
      ++bsp_rs485_rx_completed;
    }
    else
    {
      ++bsp_rs485_rx_dropped;
    }

    if (!bsp_rs485_adapter_arm_rx(NULL))
    {
      bsp_rs485_state_on_io_error(&bsp_rs485_controller);
      (void)bsp_rs485_state_poll(&bsp_rs485_controller,
                                 bsp_clock_tick_ms());
    }
  }

  return snapshot.event_mask;
}

bool bsp_rs485_is_busy(void)
{
  return bsp_rs485_controller.state != BSP_RS485_LINK_IDLE_RX;
}

bool bsp_rs485_take_received(uint8_t *destination,
                             size_t capacity,
                             size_t *received_length)
{
  bsp_rs485_rx_chunk_info_t ignored;
  return bsp_rs485_take_received_chunk(destination,
                                       capacity,
                                       received_length,
                                       &ignored);
}

bool bsp_rs485_take_received_chunk(uint8_t *destination,
                                   size_t capacity,
                                   size_t *received_length,
                                   bsp_rs485_rx_chunk_info_t *info)
{
  if ((destination == NULL) || (received_length == NULL) || (info == NULL))
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
  info->kind = bsp_rs485_rx_kind;
  info->captured_cycles = bsp_rs485_rx_captured_cycles;
  bsp_rs485_rx_ready = false;
  bsp_rs485_rx_length = 0U;
  bsp_rs485_rx_kind = BSP_RS485_RX_EVENT_NONE;
  bsp_rs485_rx_captured_cycles = 0U;
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
  diagnostics.last_hal_error = bsp_rs485_last_hal_error;
  diagnostics.irq_events =
      bsp_rs485_irq_mailbox_counters(&bsp_rs485_irq_mailbox);
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
    bsp_rs485_set_receive_immediate();
    bsp_rs485_irq_publish_from_isr(
        BSP_RS485_IRQ_EVENT_TX_COMPLETE, 0U, 0U);
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (huart != &huart1)
  {
    return;
  }

  if (HAL_UARTEx_GetRxEventType(huart) == HAL_UART_RXEVENT_HT)
  {
    bsp_rs485_irq_publish_from_isr(BSP_RS485_IRQ_EVENT_RX_HALF, size, 0U);
    return;
  }

  const bsp_rs485_rx_event_kind_t kind =
      (HAL_UARTEx_GetRxEventType(huart) == HAL_UART_RXEVENT_IDLE)
          ? BSP_RS485_RX_EVENT_IDLE
          : BSP_RS485_RX_EVENT_DMA_COMPLETE;
  bsp_rs485_irq_publish_rx_from_isr(kind,
                                   size,
                                   bsp_clock_cycle_now());
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    bsp_rs485_set_receive_immediate();
    bsp_rs485_irq_publish_from_isr(BSP_RS485_IRQ_EVENT_UART_ERROR,
                                   0U,
                                   HAL_UART_GetError(huart));
  }
}
