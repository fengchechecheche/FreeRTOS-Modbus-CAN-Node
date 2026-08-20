#ifndef APP_CAN_ACK_DIAGNOSTIC_H
#define APP_CAN_ACK_DIAGNOSTIC_H

#include <stdbool.h>
#include <stdint.h>

#define APP_CAN_ACK_DIAGNOSTIC_DLC UINT32_C(8)
#define APP_CAN_ACK_DIAGNOSTIC_ERROR_ACK UINT32_C(0x00000001)
#define APP_CAN_ACK_DIAGNOSTIC_ERROR_TX UINT32_C(0x00000002)

typedef enum {
  APP_CAN_ACK_MODE_RX_ONLY = 0,
  APP_CAN_ACK_MODE_TX_ONCE
} app_can_ack_diagnostic_mode_t;

typedef enum {
  APP_CAN_ACK_PHASE_ARMED = 0,
  APP_CAN_ACK_PHASE_RX_LATCHED,
  APP_CAN_ACK_PHASE_WAIT_TX_RESULT,
  APP_CAN_ACK_PHASE_DONE,
  APP_CAN_ACK_PHASE_TIMEOUT,
  APP_CAN_ACK_PHASE_ERROR
} app_can_ack_diagnostic_phase_t;

typedef enum {
  APP_CAN_ACK_SEND_NA = 0,
  APP_CAN_ACK_SEND_OK,
  APP_CAN_ACK_SEND_BUSY,
  APP_CAN_ACK_SEND_ERROR
} app_can_ack_diagnostic_send_result_t;

typedef struct {
  app_can_ack_diagnostic_mode_t mode;
  app_can_ack_diagnostic_phase_t phase;
  app_can_ack_diagnostic_send_result_t send_result;
  uint32_t action_deadline_ms;
  uint32_t completion_deadline_ms;
  uint32_t report_deadline_ms;
  uint32_t completion_timeout_ms;
  uint32_t report_delay_ms;
  uint32_t rx_observed;
  uint32_t rx_repeated;
  uint32_t error_flags;
  uint32_t standard_id;
  uint32_t dlc;
  uint8_t data[APP_CAN_ACK_DIAGNOSTIC_DLC];
  uint8_t data_xor;
  bool tx_approved;
  bool report_consumed;
} app_can_ack_diagnostic_t;

void app_can_ack_diagnostic_initialize(
    app_can_ack_diagnostic_t *diagnostic,
    app_can_ack_diagnostic_mode_t mode,
    uint32_t now_ms,
    uint32_t action_delay_ms,
    uint32_t completion_timeout_ms,
    uint32_t report_delay_ms);
bool app_can_ack_diagnostic_observe_rx(
    app_can_ack_diagnostic_t *diagnostic,
    uint32_t standard_id,
    uint32_t dlc,
    const uint8_t data[APP_CAN_ACK_DIAGNOSTIC_DLC],
    uint32_t now_ms);
bool app_can_ack_diagnostic_tx_due(app_can_ack_diagnostic_t *diagnostic,
                                   uint32_t now_ms);
void app_can_ack_diagnostic_record_send_result(
    app_can_ack_diagnostic_t *diagnostic,
    app_can_ack_diagnostic_send_result_t result,
    uint32_t now_ms);
void app_can_ack_diagnostic_record_tx_complete(
    app_can_ack_diagnostic_t *diagnostic,
    uint32_t now_ms);
void app_can_ack_diagnostic_record_tx_abort(
    app_can_ack_diagnostic_t *diagnostic,
    uint32_t now_ms);
void app_can_ack_diagnostic_record_error(
    app_can_ack_diagnostic_t *diagnostic,
    uint32_t error_flags,
    uint32_t now_ms);
void app_can_ack_diagnostic_poll(app_can_ack_diagnostic_t *diagnostic,
                                 uint32_t now_ms);
bool app_can_ack_diagnostic_take_report(
    app_can_ack_diagnostic_t *diagnostic);

#endif
