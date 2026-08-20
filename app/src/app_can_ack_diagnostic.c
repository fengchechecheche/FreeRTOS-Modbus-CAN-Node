#include "app_can_ack_diagnostic.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static bool app_can_ack_time_reached(uint32_t now_ms, uint32_t deadline_ms) {
  return (int32_t)(now_ms - deadline_ms) >= 0;
}

static uint32_t app_can_ack_saturating_increment(uint32_t value) {
  return value == UINT32_MAX ? UINT32_MAX : value + 1U;
}

static bool app_can_ack_is_terminal(app_can_ack_diagnostic_phase_t phase) {
  return (phase == APP_CAN_ACK_PHASE_DONE) ||
         (phase == APP_CAN_ACK_PHASE_TIMEOUT) ||
         (phase == APP_CAN_ACK_PHASE_ERROR);
}

static void app_can_ack_finish(app_can_ack_diagnostic_t *diagnostic,
                               app_can_ack_diagnostic_phase_t phase,
                               uint32_t now_ms) {
  diagnostic->phase = phase;
  diagnostic->report_deadline_ms = now_ms;
}

void app_can_ack_diagnostic_initialize(
    app_can_ack_diagnostic_t *diagnostic,
    app_can_ack_diagnostic_mode_t mode,
    uint32_t now_ms,
    uint32_t action_delay_ms,
    uint32_t completion_timeout_ms,
    uint32_t report_delay_ms) {
  if (diagnostic == NULL) {
    return;
  }

  *diagnostic = (app_can_ack_diagnostic_t){0};
  diagnostic->mode = mode;
  diagnostic->phase = APP_CAN_ACK_PHASE_ARMED;
  diagnostic->send_result = APP_CAN_ACK_SEND_NA;
  diagnostic->action_deadline_ms = now_ms + action_delay_ms;
  diagnostic->completion_timeout_ms = completion_timeout_ms;
  diagnostic->completion_deadline_ms = now_ms + completion_timeout_ms;
  diagnostic->report_delay_ms = report_delay_ms;
}

bool app_can_ack_diagnostic_observe_rx(
    app_can_ack_diagnostic_t *diagnostic,
    uint32_t standard_id,
    uint32_t dlc,
    const uint8_t data[APP_CAN_ACK_DIAGNOSTIC_DLC],
    uint32_t now_ms) {
  if ((diagnostic == NULL) || (data == NULL) ||
      (diagnostic->mode != APP_CAN_ACK_MODE_RX_ONLY) ||
      app_can_ack_is_terminal(diagnostic->phase)) {
    return false;
  }

  diagnostic->rx_observed =
      app_can_ack_saturating_increment(diagnostic->rx_observed);
  if (diagnostic->phase != APP_CAN_ACK_PHASE_ARMED) {
    diagnostic->rx_repeated =
        app_can_ack_saturating_increment(diagnostic->rx_repeated);
    return false;
  }

  diagnostic->standard_id = standard_id;
  diagnostic->dlc = dlc;
  (void)memcpy(diagnostic->data, data, sizeof(diagnostic->data));
  diagnostic->data_xor = 0U;
  for (uint32_t index = 0U; index < APP_CAN_ACK_DIAGNOSTIC_DLC; ++index) {
    diagnostic->data_xor ^= diagnostic->data[index];
  }
  diagnostic->phase = APP_CAN_ACK_PHASE_RX_LATCHED;
  diagnostic->report_deadline_ms = now_ms + diagnostic->report_delay_ms;
  return true;
}

bool app_can_ack_diagnostic_tx_due(app_can_ack_diagnostic_t *diagnostic,
                                   uint32_t now_ms) {
  if ((diagnostic == NULL) ||
      (diagnostic->mode != APP_CAN_ACK_MODE_TX_ONCE) ||
      (diagnostic->phase != APP_CAN_ACK_PHASE_ARMED) ||
      diagnostic->tx_approved ||
      !app_can_ack_time_reached(now_ms, diagnostic->action_deadline_ms)) {
    return false;
  }

  diagnostic->tx_approved = true;
  diagnostic->phase = APP_CAN_ACK_PHASE_WAIT_TX_RESULT;
  diagnostic->completion_deadline_ms =
      now_ms + diagnostic->completion_timeout_ms;
  return true;
}

void app_can_ack_diagnostic_record_send_result(
    app_can_ack_diagnostic_t *diagnostic,
    app_can_ack_diagnostic_send_result_t result,
    uint32_t now_ms) {
  if ((diagnostic == NULL) ||
      (diagnostic->mode != APP_CAN_ACK_MODE_TX_ONCE) ||
      (diagnostic->phase != APP_CAN_ACK_PHASE_WAIT_TX_RESULT) ||
      (diagnostic->send_result != APP_CAN_ACK_SEND_NA)) {
    return;
  }

  diagnostic->send_result = result;
  if ((result == APP_CAN_ACK_SEND_BUSY) ||
      (result == APP_CAN_ACK_SEND_ERROR) ||
      (result == APP_CAN_ACK_SEND_NA)) {
    app_can_ack_finish(diagnostic, APP_CAN_ACK_PHASE_ERROR, now_ms);
  }
}

void app_can_ack_diagnostic_record_tx_complete(
    app_can_ack_diagnostic_t *diagnostic,
    uint32_t now_ms) {
  if ((diagnostic == NULL) ||
      (diagnostic->mode != APP_CAN_ACK_MODE_TX_ONCE) ||
      (diagnostic->phase != APP_CAN_ACK_PHASE_WAIT_TX_RESULT) ||
      (diagnostic->send_result != APP_CAN_ACK_SEND_OK)) {
    return;
  }
  app_can_ack_finish(diagnostic, APP_CAN_ACK_PHASE_DONE, now_ms);
}

void app_can_ack_diagnostic_record_tx_abort(
    app_can_ack_diagnostic_t *diagnostic,
    uint32_t now_ms) {
  if ((diagnostic == NULL) || app_can_ack_is_terminal(diagnostic->phase)) {
    return;
  }
  diagnostic->error_flags |= APP_CAN_ACK_DIAGNOSTIC_ERROR_TX;
  app_can_ack_finish(diagnostic, APP_CAN_ACK_PHASE_ERROR, now_ms);
}

void app_can_ack_diagnostic_record_error(
    app_can_ack_diagnostic_t *diagnostic,
    uint32_t error_flags,
    uint32_t now_ms) {
  if ((diagnostic == NULL) || (error_flags == 0U) ||
      app_can_ack_is_terminal(diagnostic->phase)) {
    return;
  }
  diagnostic->error_flags |= error_flags;
  app_can_ack_finish(diagnostic, APP_CAN_ACK_PHASE_ERROR, now_ms);
}

void app_can_ack_diagnostic_poll(app_can_ack_diagnostic_t *diagnostic,
                                 uint32_t now_ms) {
  if ((diagnostic == NULL) || app_can_ack_is_terminal(diagnostic->phase)) {
    return;
  }

  if ((diagnostic->phase == APP_CAN_ACK_PHASE_RX_LATCHED) &&
      app_can_ack_time_reached(now_ms, diagnostic->report_deadline_ms)) {
    app_can_ack_finish(diagnostic, APP_CAN_ACK_PHASE_DONE, now_ms);
    return;
  }

  if ((diagnostic->mode == APP_CAN_ACK_MODE_RX_ONLY) &&
      (diagnostic->phase == APP_CAN_ACK_PHASE_ARMED) &&
      app_can_ack_time_reached(now_ms, diagnostic->completion_deadline_ms)) {
    app_can_ack_finish(diagnostic, APP_CAN_ACK_PHASE_TIMEOUT, now_ms);
    return;
  }

  if ((diagnostic->mode == APP_CAN_ACK_MODE_TX_ONCE) &&
      (diagnostic->phase == APP_CAN_ACK_PHASE_WAIT_TX_RESULT) &&
      app_can_ack_time_reached(now_ms, diagnostic->completion_deadline_ms)) {
    app_can_ack_finish(diagnostic, APP_CAN_ACK_PHASE_TIMEOUT, now_ms);
  }
}

bool app_can_ack_diagnostic_take_report(
    app_can_ack_diagnostic_t *diagnostic) {
  if ((diagnostic == NULL) || diagnostic->report_consumed ||
      !app_can_ack_is_terminal(diagnostic->phase)) {
    return false;
  }
  diagnostic->report_consumed = true;
  return true;
}
