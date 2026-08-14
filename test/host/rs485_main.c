#include "app_rs485_smoke_logic.h"
#include "bsp_rs485_state.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
  MOCK_ACTION_DE_RX = 0,
  MOCK_ACTION_DE_TX,
  MOCK_ACTION_STOP_RX,
  MOCK_ACTION_START_TX,
  MOCK_ACTION_ABORT_TX,
  MOCK_ACTION_ARM_RX
} mock_action_t;

typedef struct
{
  mock_action_t actions[16];
  size_t action_count;
  bool stop_rx_ok;
  bool start_tx_ok;
  bool arm_rx_ok;
  size_t last_tx_length;
  uint8_t last_tx_first_byte;
} mock_adapter_t;

#define CHECK(condition)          \
  do                              \
  {                               \
    if (!(condition))             \
    {                             \
      return EXIT_FAILURE;        \
    }                             \
  } while (false)

static void mock_record(mock_adapter_t *mock, mock_action_t action)
{
  if (mock->action_count < (sizeof(mock->actions) / sizeof(mock->actions[0])))
  {
    mock->actions[mock->action_count] = action;
    ++mock->action_count;
  }
}

static void mock_set_transmit(void *context, bool enabled)
{
  mock_adapter_t *mock = context;
  mock_record(mock, enabled ? MOCK_ACTION_DE_TX : MOCK_ACTION_DE_RX);
}

static bool mock_start_tx(void *context,
                          const uint8_t *data,
                          size_t length)
{
  mock_adapter_t *mock = context;
  mock_record(mock, MOCK_ACTION_START_TX);
  mock->last_tx_length = length;
  mock->last_tx_first_byte = data[0];
  return mock->start_tx_ok;
}

static bool mock_stop_rx(void *context)
{
  mock_adapter_t *mock = context;
  mock_record(mock, MOCK_ACTION_STOP_RX);
  return mock->stop_rx_ok;
}

static void mock_abort_tx(void *context)
{
  mock_adapter_t *mock = context;
  mock_record(mock, MOCK_ACTION_ABORT_TX);
}

static bool mock_arm_rx(void *context)
{
  mock_adapter_t *mock = context;
  mock_record(mock, MOCK_ACTION_ARM_RX);
  return mock->arm_rx_ok;
}

static bsp_rs485_state_ops_t mock_ops(mock_adapter_t *mock)
{
  const bsp_rs485_state_ops_t ops = {
      .context = mock,
      .set_transmit = mock_set_transmit,
      .stop_rx_dma = mock_stop_rx,
      .start_tx_dma = mock_start_tx,
      .abort_tx = mock_abort_tx,
      .arm_rx_dma = mock_arm_rx,
  };
  return ops;
}

static void mock_reset_trace(mock_adapter_t *mock)
{
  mock->action_count = 0U;
}

static int test_initialize(void)
{
  mock_adapter_t mock = {
      .stop_rx_ok = true, .start_tx_ok = true, .arm_rx_ok = true};
  bsp_rs485_state_controller_t controller;
  const bsp_rs485_state_ops_t ops = mock_ops(&mock);

  CHECK(bsp_rs485_state_initialize(&controller, &ops) ==
        BSP_RS485_RESULT_OK);
  CHECK(controller.state == BSP_RS485_LINK_IDLE_RX);
  CHECK(mock.action_count == 2U);
  CHECK(mock.actions[0] == MOCK_ACTION_DE_RX);
  CHECK(mock.actions[1] == MOCK_ACTION_ARM_RX);
  return EXIT_SUCCESS;
}

static int test_send_and_final_complete_order(void)
{
  mock_adapter_t mock = {
      .stop_rx_ok = true, .start_tx_ok = true, .arm_rx_ok = true};
  bsp_rs485_state_controller_t controller;
  const bsp_rs485_state_ops_t ops = mock_ops(&mock);
  const uint8_t payload[] = {0x50U, 0x35U};
  CHECK(bsp_rs485_state_initialize(&controller, &ops) ==
        BSP_RS485_RESULT_OK);
  mock_reset_trace(&mock);

  CHECK(bsp_rs485_state_send(&controller, payload, sizeof(payload), 10U) ==
        BSP_RS485_RESULT_OK);
  CHECK(controller.state == BSP_RS485_LINK_TX_ACTIVE);
  CHECK(mock.action_count == 3U);
  CHECK(mock.actions[0] == MOCK_ACTION_STOP_RX);
  CHECK(mock.actions[1] == MOCK_ACTION_DE_TX);
  CHECK(mock.actions[2] == MOCK_ACTION_START_TX);
  CHECK(mock.last_tx_length == sizeof(payload));
  CHECK(mock.last_tx_first_byte == payload[0]);

  bsp_rs485_state_on_tx_complete(&controller);
  CHECK(controller.state == BSP_RS485_LINK_IDLE_RX);
  CHECK(mock.action_count == 5U);
  CHECK(mock.actions[3] == MOCK_ACTION_DE_RX);
  CHECK(mock.actions[4] == MOCK_ACTION_ARM_RX);
  CHECK(controller.counters.tx_started == 1U);
  CHECK(controller.counters.tx_completed == 1U);
  return EXIT_SUCCESS;
}

static int test_invalid_and_busy_rejection(void)
{
  mock_adapter_t mock = {
      .stop_rx_ok = true, .start_tx_ok = true, .arm_rx_ok = true};
  bsp_rs485_state_controller_t controller;
  const bsp_rs485_state_ops_t ops = mock_ops(&mock);
  uint8_t payload[BSP_RS485_MAX_FRAME_SIZE + 1U] = {0U};
  CHECK(bsp_rs485_state_initialize(&controller, &ops) ==
        BSP_RS485_RESULT_OK);
  mock_reset_trace(&mock);

  CHECK(bsp_rs485_state_send(&controller, payload, 0U, 0U) ==
        BSP_RS485_RESULT_INVALID_ARGUMENT);
  CHECK(bsp_rs485_state_send(&controller, payload, sizeof(payload), 0U) ==
        BSP_RS485_RESULT_INVALID_ARGUMENT);
  CHECK(bsp_rs485_state_send(&controller, payload, 1U, 0U) ==
        BSP_RS485_RESULT_OK);
  const size_t action_count = mock.action_count;
  CHECK(bsp_rs485_state_send(&controller, payload, 1U, 1U) ==
        BSP_RS485_RESULT_BUSY);
  CHECK(mock.action_count == action_count);
  return EXIT_SUCCESS;
}

static int test_start_failure_restores_receive(void)
{
  mock_adapter_t mock = {
      .stop_rx_ok = true, .start_tx_ok = false, .arm_rx_ok = true};
  bsp_rs485_state_controller_t controller;
  const bsp_rs485_state_ops_t ops = mock_ops(&mock);
  const uint8_t payload = 0x50U;
  CHECK(bsp_rs485_state_initialize(&controller, &ops) ==
        BSP_RS485_RESULT_OK);
  mock_reset_trace(&mock);

  CHECK(bsp_rs485_state_send(&controller, &payload, 1U, 0U) ==
        BSP_RS485_RESULT_IO_ERROR);
  CHECK(controller.state == BSP_RS485_LINK_IDLE_RX);
  CHECK(mock.action_count == 5U);
  CHECK(mock.actions[0] == MOCK_ACTION_STOP_RX);
  CHECK(mock.actions[1] == MOCK_ACTION_DE_TX);
  CHECK(mock.actions[2] == MOCK_ACTION_START_TX);
  CHECK(mock.actions[3] == MOCK_ACTION_DE_RX);
  CHECK(mock.actions[4] == MOCK_ACTION_ARM_RX);
  CHECK(controller.last_error == BSP_RS485_ERROR_TX_START);
  return EXIT_SUCCESS;
}

static int test_wrap_safe_timeout(void)
{
  mock_adapter_t mock = {
      .stop_rx_ok = true, .start_tx_ok = true, .arm_rx_ok = true};
  bsp_rs485_state_controller_t controller;
  const bsp_rs485_state_ops_t ops = mock_ops(&mock);
  const uint8_t payload = 0x50U;
  CHECK(bsp_rs485_state_initialize(&controller, &ops) ==
        BSP_RS485_RESULT_OK);
  mock_reset_trace(&mock);

  CHECK(bsp_rs485_state_send(&controller,
                              &payload,
                              1U,
                              UINT32_C(0xfffffff5)) == BSP_RS485_RESULT_OK);
  CHECK(bsp_rs485_state_poll(&controller, UINT32_C(0x00000030)) ==
        BSP_RS485_RESULT_TIMEOUT);
  CHECK(controller.state == BSP_RS485_LINK_IDLE_RX);
  CHECK(mock.actions[3] == MOCK_ACTION_ABORT_TX);
  CHECK(mock.actions[4] == MOCK_ACTION_DE_RX);
  CHECK(mock.actions[5] == MOCK_ACTION_ARM_RX);
  CHECK(controller.counters.tx_timeouts == 1U);
  return EXIT_SUCCESS;
}

static int test_uart_error_recovery(void)
{
  mock_adapter_t mock = {
      .stop_rx_ok = true, .start_tx_ok = true, .arm_rx_ok = true};
  bsp_rs485_state_controller_t controller;
  const bsp_rs485_state_ops_t ops = mock_ops(&mock);
  const uint8_t payload = 0x50U;
  CHECK(bsp_rs485_state_initialize(&controller, &ops) ==
        BSP_RS485_RESULT_OK);
  CHECK(bsp_rs485_state_send(&controller, &payload, 1U, 0U) ==
        BSP_RS485_RESULT_OK);
  mock_reset_trace(&mock);

  bsp_rs485_state_on_io_error(&controller);
  CHECK(controller.state == BSP_RS485_LINK_FAULT_RECOVERY);
  CHECK(mock.action_count == 1U);
  CHECK(mock.actions[0] == MOCK_ACTION_DE_RX);
  CHECK(bsp_rs485_state_poll(&controller, 1U) == BSP_RS485_RESULT_IO_ERROR);
  CHECK(controller.state == BSP_RS485_LINK_IDLE_RX);
  CHECK(mock.actions[1] == MOCK_ACTION_ABORT_TX);
  CHECK(mock.actions[2] == MOCK_ACTION_DE_RX);
  CHECK(mock.actions[3] == MOCK_ACTION_ARM_RX);
  return EXIT_SUCCESS;
}

static int test_rx_rearm_failure_is_bounded(void)
{
  mock_adapter_t mock = {
      .stop_rx_ok = true, .start_tx_ok = true, .arm_rx_ok = false};
  bsp_rs485_state_controller_t controller;
  const bsp_rs485_state_ops_t ops = mock_ops(&mock);

  CHECK(bsp_rs485_state_initialize(&controller, &ops) ==
        BSP_RS485_RESULT_IO_ERROR);
  CHECK(controller.state == BSP_RS485_LINK_FAULT_RECOVERY);
  CHECK(controller.last_error == BSP_RS485_ERROR_RX_REARM);
  mock.arm_rx_ok = true;
  CHECK(bsp_rs485_state_poll(&controller, 0U) == BSP_RS485_RESULT_IO_ERROR);
  CHECK(controller.state == BSP_RS485_LINK_IDLE_RX);
  return EXIT_SUCCESS;
}

static int test_stop_rx_failure_is_fail_closed(void)
{
  mock_adapter_t mock = {
      .stop_rx_ok = false, .start_tx_ok = true, .arm_rx_ok = true};
  bsp_rs485_state_controller_t controller;
  const bsp_rs485_state_ops_t ops = mock_ops(&mock);
  uint8_t payload[BSP_RS485_MAX_FRAME_SIZE] = {0x50U};
  CHECK(bsp_rs485_state_initialize(&controller, &ops) ==
        BSP_RS485_RESULT_OK);
  mock_reset_trace(&mock);

  CHECK(bsp_rs485_state_send(&controller,
                              payload,
                              sizeof(payload),
                              0U) == BSP_RS485_RESULT_IO_ERROR);
  CHECK(controller.state == BSP_RS485_LINK_IDLE_RX);
  CHECK(controller.last_error == BSP_RS485_ERROR_RX_STOP);
  CHECK(controller.counters.rx_stop_failures == 1U);
  CHECK(mock.action_count == 1U);
  CHECK(mock.actions[0] == MOCK_ACTION_STOP_RX);
  return EXIT_SUCCESS;
}

static int test_fixed_smoke_bytes(void)
{
  size_t request_length = 0U;
  const uint8_t *request = app_rs485_smoke_request(&request_length);
  uint8_t response[8] = {0U};
  size_t response_length = 0U;
  CHECK(request != NULL);
  CHECK(request_length == 5U);
  CHECK(app_rs485_smoke_build_response(request,
                                       request_length,
                                       response,
                                       sizeof(response),
                                       &response_length));
  CHECK(response_length == 7U);
  CHECK(memcmp(response, "P5T03OK", response_length) == 0);

  const uint8_t non_matching[] = {'P', '5', 'T', '0', '4'};
  CHECK(!app_rs485_smoke_build_response(non_matching,
                                        sizeof(non_matching),
                                        response,
                                        sizeof(response),
                                        &response_length));
  CHECK(response_length == 0U);
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(test_initialize() == EXIT_SUCCESS);
  CHECK(test_send_and_final_complete_order() == EXIT_SUCCESS);
  CHECK(test_invalid_and_busy_rejection() == EXIT_SUCCESS);
  CHECK(test_start_failure_restores_receive() == EXIT_SUCCESS);
  CHECK(test_wrap_safe_timeout() == EXIT_SUCCESS);
  CHECK(test_uart_error_recovery() == EXIT_SUCCESS);
  CHECK(test_rx_rearm_failure_is_bounded() == EXIT_SUCCESS);
  CHECK(test_stop_rx_failure_is_fail_closed() == EXIT_SUCCESS);
  CHECK(test_fixed_smoke_bytes() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
