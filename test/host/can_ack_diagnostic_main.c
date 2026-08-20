#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "app_can_ack_diagnostic.h"

#define CHECK(condition)                                                     \
  do {                                                                       \
    if (!(condition)) {                                                      \
      (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,       \
                    __LINE__, #condition);                                   \
      return EXIT_FAILURE;                                                   \
    }                                                                        \
  } while (0)

static const uint8_t test_rx_data[APP_CAN_ACK_DIAGNOSTIC_DLC] = {
    UINT8_C(0xA5), UINT8_C(0x5A), 0U, 0U, 0U, 0U, 0U, 0U};

static int test_rx_latches_once_and_reports_once(void) {
  app_can_ack_diagnostic_t diagnostic;
  app_can_ack_diagnostic_initialize(&diagnostic, APP_CAN_ACK_MODE_RX_ONLY,
                                    0U, 0U, 3000U, 250U);

  CHECK(app_can_ack_diagnostic_observe_rx(&diagnostic, UINT32_C(0x240), 8U,
                                          test_rx_data, 10U));
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_RX_LATCHED);
  CHECK(diagnostic.standard_id == UINT32_C(0x240));
  CHECK(diagnostic.dlc == 8U);
  CHECK(diagnostic.data[0] == UINT8_C(0xA5));
  CHECK(diagnostic.data[1] == UINT8_C(0x5A));
  CHECK(diagnostic.data_xor == UINT8_C(0xFF));
  CHECK(diagnostic.rx_observed == 1U);
  CHECK(diagnostic.rx_repeated == 0U);

  for (uint32_t repeat = 0U; repeat < 100U; ++repeat) {
    CHECK(!app_can_ack_diagnostic_observe_rx(
        &diagnostic, UINT32_C(0x240), 8U, test_rx_data, 11U + repeat));
    if (repeat == 0U) {
      CHECK(diagnostic.rx_repeated == 1U);
    }
    if (repeat == 9U) {
      CHECK(diagnostic.rx_repeated == 10U);
    }
  }
  CHECK(diagnostic.rx_observed == 101U);
  CHECK(diagnostic.rx_repeated == 100U);
  CHECK(!diagnostic.tx_approved);

  app_can_ack_diagnostic_poll(&diagnostic, 259U);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_RX_LATCHED);
  CHECK(!app_can_ack_diagnostic_take_report(&diagnostic));
  app_can_ack_diagnostic_poll(&diagnostic, 260U);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_DONE);
  CHECK(app_can_ack_diagnostic_take_report(&diagnostic));
  CHECK(!app_can_ack_diagnostic_take_report(&diagnostic));
  return EXIT_SUCCESS;
}

static int test_rx_timeout_and_error(void) {
  app_can_ack_diagnostic_t diagnostic;
  app_can_ack_diagnostic_initialize(&diagnostic, APP_CAN_ACK_MODE_RX_ONLY,
                                    100U, 0U, 3000U, 250U);
  app_can_ack_diagnostic_poll(&diagnostic, 3099U);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_ARMED);
  app_can_ack_diagnostic_poll(&diagnostic, 3100U);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_TIMEOUT);
  CHECK(app_can_ack_diagnostic_take_report(&diagnostic));

  app_can_ack_diagnostic_initialize(&diagnostic, APP_CAN_ACK_MODE_RX_ONLY,
                                    0U, 0U, 3000U, 250U);
  app_can_ack_diagnostic_record_error(
      &diagnostic, APP_CAN_ACK_DIAGNOSTIC_ERROR_ACK, 50U);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_ERROR);
  CHECK(diagnostic.error_flags == APP_CAN_ACK_DIAGNOSTIC_ERROR_ACK);
  CHECK(app_can_ack_diagnostic_take_report(&diagnostic));
  return EXIT_SUCCESS;
}

static int test_tx_attempt_and_complete_once(void) {
  app_can_ack_diagnostic_t diagnostic;
  app_can_ack_diagnostic_initialize(&diagnostic, APP_CAN_ACK_MODE_TX_ONCE,
                                    100U, 2000U, 1000U, 0U);
  CHECK(!app_can_ack_diagnostic_tx_due(&diagnostic, 2099U));
  CHECK(app_can_ack_diagnostic_tx_due(&diagnostic, 2100U));
  CHECK(diagnostic.tx_approved);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_WAIT_TX_RESULT);
  CHECK(!app_can_ack_diagnostic_tx_due(&diagnostic, 2100U));

  app_can_ack_diagnostic_record_send_result(
      &diagnostic, APP_CAN_ACK_SEND_OK, 2100U);
  CHECK(diagnostic.send_result == APP_CAN_ACK_SEND_OK);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_WAIT_TX_RESULT);
  app_can_ack_diagnostic_record_tx_complete(&diagnostic, 2200U);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_DONE);
  CHECK(!app_can_ack_diagnostic_tx_due(&diagnostic, 5000U));
  CHECK(app_can_ack_diagnostic_take_report(&diagnostic));
  CHECK(!app_can_ack_diagnostic_take_report(&diagnostic));
  return EXIT_SUCCESS;
}

static int test_tx_terminal_results_do_not_retry(void) {
  static const app_can_ack_diagnostic_send_result_t results[] = {
      APP_CAN_ACK_SEND_BUSY, APP_CAN_ACK_SEND_ERROR};
  for (uint32_t index = 0U; index < (sizeof(results) / sizeof(results[0]));
       ++index) {
    app_can_ack_diagnostic_t diagnostic;
    app_can_ack_diagnostic_initialize(&diagnostic, APP_CAN_ACK_MODE_TX_ONCE,
                                      0U, 0U, 1000U, 0U);
    CHECK(app_can_ack_diagnostic_tx_due(&diagnostic, 0U));
    app_can_ack_diagnostic_record_send_result(&diagnostic, results[index],
                                              0U);
    CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_ERROR);
    CHECK(!app_can_ack_diagnostic_tx_due(&diagnostic, 5000U));
    CHECK(app_can_ack_diagnostic_take_report(&diagnostic));
  }
  return EXIT_SUCCESS;
}

static int test_tx_abort_ack_error_and_timeout(void) {
  app_can_ack_diagnostic_t diagnostic;
  app_can_ack_diagnostic_initialize(&diagnostic, APP_CAN_ACK_MODE_TX_ONCE,
                                    0U, 0U, 1000U, 0U);
  CHECK(app_can_ack_diagnostic_tx_due(&diagnostic, 0U));
  app_can_ack_diagnostic_record_send_result(
      &diagnostic, APP_CAN_ACK_SEND_OK, 0U);
  app_can_ack_diagnostic_record_tx_abort(&diagnostic, 1U);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_ERROR);
  CHECK((diagnostic.error_flags & APP_CAN_ACK_DIAGNOSTIC_ERROR_TX) != 0U);

  app_can_ack_diagnostic_initialize(&diagnostic, APP_CAN_ACK_MODE_TX_ONCE,
                                    0U, 0U, 1000U, 0U);
  CHECK(app_can_ack_diagnostic_tx_due(&diagnostic, 0U));
  app_can_ack_diagnostic_record_send_result(
      &diagnostic, APP_CAN_ACK_SEND_OK, 0U);
  app_can_ack_diagnostic_record_error(
      &diagnostic, APP_CAN_ACK_DIAGNOSTIC_ERROR_ACK, 1U);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_ERROR);
  CHECK((diagnostic.error_flags & APP_CAN_ACK_DIAGNOSTIC_ERROR_ACK) != 0U);

  app_can_ack_diagnostic_initialize(&diagnostic, APP_CAN_ACK_MODE_TX_ONCE,
                                    0U, 0U, 1000U, 0U);
  CHECK(app_can_ack_diagnostic_tx_due(&diagnostic, 0U));
  app_can_ack_diagnostic_record_send_result(
      &diagnostic, APP_CAN_ACK_SEND_OK, 0U);
  app_can_ack_diagnostic_poll(&diagnostic, 999U);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_WAIT_TX_RESULT);
  app_can_ack_diagnostic_poll(&diagnostic, 1000U);
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_TIMEOUT);
  CHECK(!app_can_ack_diagnostic_tx_due(&diagnostic, 1001U));
  return EXIT_SUCCESS;
}

static int test_counter_saturation_and_tick_wrap(void) {
  app_can_ack_diagnostic_t diagnostic;
  app_can_ack_diagnostic_initialize(&diagnostic, APP_CAN_ACK_MODE_RX_ONLY,
                                    0U, 0U, 3000U, 250U);
  diagnostic.phase = APP_CAN_ACK_PHASE_RX_LATCHED;
  diagnostic.rx_observed = UINT32_MAX;
  diagnostic.rx_repeated = UINT32_MAX;
  CHECK(!app_can_ack_diagnostic_observe_rx(&diagnostic, UINT32_C(0x240), 8U,
                                           test_rx_data, 0U));
  CHECK(diagnostic.rx_observed == UINT32_MAX);
  CHECK(diagnostic.rx_repeated == UINT32_MAX);

  const uint32_t near_wrap = UINT32_C(0xFFFFFF00);
  app_can_ack_diagnostic_initialize(&diagnostic, APP_CAN_ACK_MODE_RX_ONLY,
                                    near_wrap, 0U, 512U, 250U);
  app_can_ack_diagnostic_poll(&diagnostic, UINT32_C(0x000000FF));
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_ARMED);
  app_can_ack_diagnostic_poll(&diagnostic, UINT32_C(0x00000100));
  CHECK(diagnostic.phase == APP_CAN_ACK_PHASE_TIMEOUT);

  app_can_ack_diagnostic_initialize(&diagnostic, APP_CAN_ACK_MODE_TX_ONCE,
                                    near_wrap, 512U, 1000U, 0U);
  CHECK(!app_can_ack_diagnostic_tx_due(&diagnostic, UINT32_C(0x000000FF)));
  CHECK(app_can_ack_diagnostic_tx_due(&diagnostic, UINT32_C(0x00000100)));
  CHECK(!app_can_ack_diagnostic_tx_due(&diagnostic, UINT32_C(0x00000101)));
  return EXIT_SUCCESS;
}

int main(void) {
  CHECK(test_rx_latches_once_and_reports_once() == EXIT_SUCCESS);
  CHECK(test_rx_timeout_and_error() == EXIT_SUCCESS);
  CHECK(test_tx_attempt_and_complete_once() == EXIT_SUCCESS);
  CHECK(test_tx_terminal_results_do_not_retry() == EXIT_SUCCESS);
  CHECK(test_tx_abort_ack_error_and_timeout() == EXIT_SUCCESS);
  CHECK(test_counter_saturation_and_tick_wrap() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
