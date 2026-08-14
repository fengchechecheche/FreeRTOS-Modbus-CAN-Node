#ifndef BSP_RS485_STATE_H
#define BSP_RS485_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BSP_RS485_RX_DMA_CHUNK_SIZE (64U)
#define BSP_RS485_MAX_FRAME_SIZE (256U)

typedef enum
{
  BSP_RS485_LINK_IDLE_RX = 0,
  BSP_RS485_LINK_TX_ACTIVE,
  BSP_RS485_LINK_FAULT_RECOVERY
} bsp_rs485_link_state_t;

typedef enum
{
  BSP_RS485_RESULT_OK = 0,
  BSP_RS485_RESULT_INVALID_ARGUMENT,
  BSP_RS485_RESULT_BUSY,
  BSP_RS485_RESULT_IO_ERROR,
  BSP_RS485_RESULT_TIMEOUT
} bsp_rs485_result_t;

typedef enum
{
  BSP_RS485_ERROR_NONE = 0,
  BSP_RS485_ERROR_RX_STOP,
  BSP_RS485_ERROR_TX_START,
  BSP_RS485_ERROR_TX_TIMEOUT,
  BSP_RS485_ERROR_UART,
  BSP_RS485_ERROR_RX_REARM
} bsp_rs485_error_t;

typedef struct
{
  uint32_t tx_started;
  uint32_t tx_completed;
  uint32_t rx_stop_failures;
  uint32_t tx_start_failures;
  uint32_t tx_timeouts;
  uint32_t uart_errors;
  uint32_t rx_rearm_failures;
} bsp_rs485_state_counters_t;

typedef struct
{
  void *context;
  void (*set_transmit)(void *context, bool enabled);
  bool (*stop_rx_dma)(void *context);
  bool (*start_tx_dma)(void *context, const uint8_t *data, size_t length);
  void (*abort_tx)(void *context);
  bool (*arm_rx_dma)(void *context);
} bsp_rs485_state_ops_t;

typedef struct
{
  bsp_rs485_state_ops_t ops;
  volatile bsp_rs485_link_state_t state;
  volatile bsp_rs485_error_t last_error;
  bsp_rs485_state_counters_t counters;
  uint8_t tx_buffer[BSP_RS485_MAX_FRAME_SIZE];
  size_t tx_length;
  uint32_t tx_started_at_ms;
  uint32_t tx_timeout_ms;
} bsp_rs485_state_controller_t;

bsp_rs485_result_t bsp_rs485_state_initialize(
    bsp_rs485_state_controller_t *controller,
    const bsp_rs485_state_ops_t *ops);
bsp_rs485_result_t bsp_rs485_state_send(
    bsp_rs485_state_controller_t *controller,
    const uint8_t *data,
    size_t length,
    uint32_t now_ms);
void bsp_rs485_state_on_tx_complete(
    bsp_rs485_state_controller_t *controller);
void bsp_rs485_state_on_io_error(
    bsp_rs485_state_controller_t *controller);
bsp_rs485_result_t bsp_rs485_state_poll(
    bsp_rs485_state_controller_t *controller,
    uint32_t now_ms);
uint32_t bsp_rs485_state_timeout_ms(size_t length);

#endif
