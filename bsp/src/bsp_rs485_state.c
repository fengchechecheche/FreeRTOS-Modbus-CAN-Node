#include "bsp_rs485_state.h"

#include <string.h>

#define BSP_RS485_BITS_PER_CHARACTER (11U)
#define BSP_RS485_BAUD_RATE (19200U)
#define BSP_RS485_TIMEOUT_MARGIN_MS (20U)

static bool bsp_rs485_state_ops_are_valid(const bsp_rs485_state_ops_t *ops)
{
  return (ops != NULL) && (ops->set_transmit != NULL) &&
         (ops->start_tx_dma != NULL) && (ops->abort_tx != NULL) &&
         (ops->arm_rx_dma != NULL);
}

static bool bsp_rs485_state_restore_receive(
    bsp_rs485_state_controller_t *controller)
{
  controller->ops.set_transmit(controller->ops.context, false);
  if (!controller->ops.arm_rx_dma(controller->ops.context))
  {
    ++controller->counters.rx_rearm_failures;
    controller->last_error = BSP_RS485_ERROR_RX_REARM;
    controller->state = BSP_RS485_LINK_FAULT_RECOVERY;
    return false;
  }

  controller->state = BSP_RS485_LINK_IDLE_RX;
  return true;
}

uint32_t bsp_rs485_state_timeout_ms(size_t length)
{
  if (length > BSP_RS485_MAX_FRAME_SIZE)
  {
    length = BSP_RS485_MAX_FRAME_SIZE;
  }

  const uint32_t total_bits =
      (uint32_t)length * BSP_RS485_BITS_PER_CHARACTER;
  const uint32_t line_time_ms =
      ((total_bits * UINT32_C(1000)) + (BSP_RS485_BAUD_RATE - 1U)) /
      BSP_RS485_BAUD_RATE;
  return line_time_ms + BSP_RS485_TIMEOUT_MARGIN_MS;
}

bsp_rs485_result_t bsp_rs485_state_initialize(
    bsp_rs485_state_controller_t *controller,
    const bsp_rs485_state_ops_t *ops)
{
  if ((controller == NULL) || !bsp_rs485_state_ops_are_valid(ops))
  {
    return BSP_RS485_RESULT_INVALID_ARGUMENT;
  }

  (void)memset(controller, 0, sizeof(*controller));
  controller->ops = *ops;
  controller->state = BSP_RS485_LINK_IDLE_RX;
  controller->last_error = BSP_RS485_ERROR_NONE;
  controller->ops.set_transmit(controller->ops.context, false);

  if (!controller->ops.arm_rx_dma(controller->ops.context))
  {
    ++controller->counters.rx_rearm_failures;
    controller->last_error = BSP_RS485_ERROR_RX_REARM;
    controller->state = BSP_RS485_LINK_FAULT_RECOVERY;
    return BSP_RS485_RESULT_IO_ERROR;
  }

  return BSP_RS485_RESULT_OK;
}

bsp_rs485_result_t bsp_rs485_state_send(
    bsp_rs485_state_controller_t *controller,
    const uint8_t *data,
    size_t length,
    uint32_t now_ms)
{
  if ((controller == NULL) || (data == NULL) || (length == 0U) ||
      (length > BSP_RS485_MAX_FRAME_SIZE))
  {
    return BSP_RS485_RESULT_INVALID_ARGUMENT;
  }

  if (controller->state != BSP_RS485_LINK_IDLE_RX)
  {
    return BSP_RS485_RESULT_BUSY;
  }

  (void)memcpy(controller->tx_buffer, data, length);
  controller->tx_length = length;
  controller->tx_started_at_ms = now_ms;
  controller->tx_timeout_ms = bsp_rs485_state_timeout_ms(length);
  controller->last_error = BSP_RS485_ERROR_NONE;
  controller->state = BSP_RS485_LINK_TX_ACTIVE;

  controller->ops.set_transmit(controller->ops.context, true);
  if (!controller->ops.start_tx_dma(controller->ops.context,
                                    controller->tx_buffer,
                                    controller->tx_length))
  {
    ++controller->counters.tx_start_failures;
    controller->last_error = BSP_RS485_ERROR_TX_START;
    (void)bsp_rs485_state_restore_receive(controller);
    return BSP_RS485_RESULT_IO_ERROR;
  }

  ++controller->counters.tx_started;
  return BSP_RS485_RESULT_OK;
}

void bsp_rs485_state_on_tx_complete(
    bsp_rs485_state_controller_t *controller)
{
  if ((controller == NULL) ||
      (controller->state != BSP_RS485_LINK_TX_ACTIVE))
  {
    return;
  }

  ++controller->counters.tx_completed;
  (void)bsp_rs485_state_restore_receive(controller);
}

void bsp_rs485_state_on_io_error(
    bsp_rs485_state_controller_t *controller)
{
  if (controller == NULL)
  {
    return;
  }

  ++controller->counters.uart_errors;
  controller->last_error = BSP_RS485_ERROR_UART;
  controller->ops.set_transmit(controller->ops.context, false);
  controller->state = BSP_RS485_LINK_FAULT_RECOVERY;
}

bsp_rs485_result_t bsp_rs485_state_poll(
    bsp_rs485_state_controller_t *controller,
    uint32_t now_ms)
{
  if (controller == NULL)
  {
    return BSP_RS485_RESULT_INVALID_ARGUMENT;
  }

  if (controller->state == BSP_RS485_LINK_FAULT_RECOVERY)
  {
    controller->ops.abort_tx(controller->ops.context);
    if (!bsp_rs485_state_restore_receive(controller))
    {
      return BSP_RS485_RESULT_IO_ERROR;
    }
    return BSP_RS485_RESULT_IO_ERROR;
  }

  if (controller->state == BSP_RS485_LINK_TX_ACTIVE)
  {
    const uint32_t elapsed_ms = now_ms - controller->tx_started_at_ms;
    if (elapsed_ms >= controller->tx_timeout_ms)
    {
      controller->ops.abort_tx(controller->ops.context);
      ++controller->counters.tx_timeouts;
      controller->last_error = BSP_RS485_ERROR_TX_TIMEOUT;
      (void)bsp_rs485_state_restore_receive(controller);
      return BSP_RS485_RESULT_TIMEOUT;
    }
  }

  return BSP_RS485_RESULT_OK;
}
