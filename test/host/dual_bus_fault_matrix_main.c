#include "app_can_runtime.h"
#include "app_health_policy.h"
#include "app_task_model.h"
#include "app_transport_policy.h"
#include "bsp_rs485_state.h"
#include "p5_modbus_rtu_adu.h"
#include "p5_modbus_rtu_stream.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(id, condition)                                                   \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "%s FAIL line %d: %s\n", id, __LINE__,           \
                    #condition);                                               \
      return EXIT_FAILURE;                                                     \
    }                                                                          \
  } while (false)

enum {
  CHARACTER_TICKS = 10,
  T1_5_TICKS = 15,
  T3_5_TICKS = 35,
};

typedef struct {
  bool stop_rx_ok;
  bool start_tx_ok;
  bool arm_rx_ok;
  bool transmit_enabled;
  uint32_t abort_count;
  uint32_t arm_count;
} rs485_mock_t;

typedef struct {
  uint32_t accepted;
} modbus_trace_t;

typedef struct {
  rs485_mock_t mock;
  bsp_rs485_state_controller_t rs485;
  modbus_trace_t modbus;
  p5_modbus_rtu_stream_t stream;
  app_can_tx_scheduler_t can_tx;
  app_can_controller_t can_controller;
  app_task_runtime_t tasks[APP_TASK_COUNT];
  app_transport_counters_t transport;
  app_health_policy_t health_policy;
  app_health_input_t health_input;
  app_health_decision_t health_decision;
} harness_t;

static void set_transmit(void *context, bool enabled) {
  rs485_mock_t *mock = context;
  mock->transmit_enabled = enabled;
}

static bool stop_rx(void *context) {
  const rs485_mock_t *mock = context;
  return mock->stop_rx_ok;
}

static bool start_tx(void *context, const uint8_t *data, size_t length) {
  const rs485_mock_t *mock = context;
  return mock->start_tx_ok && (data != NULL) && (length != 0U);
}

static void abort_tx(void *context) {
  rs485_mock_t *mock = context;
  ++mock->abort_count;
}

static bool arm_rx(void *context) {
  rs485_mock_t *mock = context;
  ++mock->arm_count;
  return mock->arm_rx_ok;
}

static void consume_modbus(void *context, const uint8_t *frame,
                           size_t frame_length,
                           const p5_modbus_adu_view_t *view) {
  modbus_trace_t *trace = context;
  (void)frame;
  (void)frame_length;
  (void)view;
  ++trace->accepted;
}

static uint32_t rs485_errors(const harness_t *harness) {
  const bsp_rs485_state_counters_t *counters = &harness->rs485.counters;
  return counters->rx_stop_failures + counters->tx_start_failures +
         counters->tx_timeouts + counters->uart_errors +
         counters->rx_rearm_failures;
}

static bool evaluate_health(harness_t *harness, uint32_t pending,
                            uint32_t maximum) {
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index) {
    harness->health_input.task_release_count[index] =
        harness->tasks[index].release_count;
    harness->health_input.task_deadline_miss_count[index] =
        harness->tasks[index].deadline_miss_count;
    harness->health_input.task_budget_overrun_count[index] =
        harness->tasks[index].budget_overrun_count;
  }
  harness->health_input.queue_dropped_count =
      harness->transport.event_dropped_full_count;
  harness->health_input.queue_current_pending = pending;
  harness->health_input.queue_maximum_pending = maximum;
  harness->health_input.queue_depth = APP_TRANSPORT_EVENT_QUEUE_DEPTH;
  harness->health_input.rs485_error_count = rs485_errors(harness);
  return app_health_policy_evaluate(&harness->health_policy,
                                    &harness->health_input,
                                    &harness->health_decision);
}

static bool initialize(harness_t *harness) {
  (void)memset(harness, 0, sizeof(*harness));
  harness->mock.stop_rx_ok = true;
  harness->mock.start_tx_ok = true;
  harness->mock.arm_rx_ok = true;
  const bsp_rs485_state_ops_t ops = {
      .context = &harness->mock,
      .set_transmit = set_transmit,
      .stop_rx_dma = stop_rx,
      .start_tx_dma = start_tx,
      .abort_tx = abort_tx,
      .arm_rx_dma = arm_rx,
  };
  if (bsp_rs485_state_initialize(&harness->rs485, &ops) !=
      BSP_RS485_RESULT_OK) {
    return false;
  }
  const p5_modbus_rtu_stream_config_t config = {
      .character_ticks = CHARACTER_TICKS,
      .t1_5_ticks = T1_5_TICKS,
      .t3_5_ticks = T3_5_TICKS,
      .consumer = consume_modbus,
      .consumer_context = &harness->modbus,
  };
  if (!p5_modbus_rtu_stream_initialize(&harness->stream, &config)) {
    return false;
  }
  app_can_tx_scheduler_initialize(&harness->can_tx);
  app_can_controller_initialize(&harness->can_controller);
  if (!app_can_controller_begin_initial_start(&harness->can_controller) ||
      !app_can_controller_complete_start(&harness->can_controller, true, 0U)) {
    return false;
  }
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index) {
    app_task_runtime_initialize(&harness->tasks[index], 0U);
  }
  app_transport_counters_initialize(&harness->transport);
  app_health_policy_initialize(&harness->health_policy);
  return evaluate_health(harness, 0U, 0U);
}

static bool advance_tasks(harness_t *harness) {
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index) {
    const app_task_contract_t *contract =
        app_task_model_contract((app_task_id_t)index);
    if (contract == NULL) {
      return false;
    }
    const uint32_t start = harness->tasks[index].next_release_tick;
    if (!app_task_runtime_record_cycle(&harness->tasks[index], contract, start,
                                       start + contract->execution_budget_ms)) {
      return false;
    }
  }
  return true;
}

static size_t make_request(uint8_t *frame) {
  const uint8_t data[] = {0U, 0U, 0U, 2U};
  size_t length = 0U;
  if (p5_modbus_rtu_adu_encode(4U, 4U, data, sizeof(data), frame,
                                P5_MODBUS_RTU_MAX_ADU_SIZE, &length) !=
      P5_MODBUS_ADU_OK) {
    return 0U;
  }
  return length;
}

static uint32_t push_bytes(p5_modbus_rtu_stream_t *stream,
                           const uint8_t *frame, size_t length,
                           uint32_t first_end) {
  uint32_t end = first_end;
  for (size_t index = 0U; index < length; ++index) {
    p5_modbus_rtu_stream_push_byte(stream, frame[index], end);
    end += CHARACTER_TICKS;
  }
  return end - CHARACTER_TICKS;
}

static bool feed_request(harness_t *harness, uint32_t first_end,
                         bool corrupt_crc) {
  uint8_t frame[P5_MODBUS_RTU_MAX_ADU_SIZE];
  const size_t length = make_request(frame);
  if (length == 0U) {
    return false;
  }
  if (corrupt_crc) {
    frame[length - 1U] ^= 1U;
  }
  const uint32_t last = push_bytes(&harness->stream, frame, length, first_end);
  p5_modbus_rtu_stream_poll(&harness->stream, last + T3_5_TICKS);
  return true;
}

static bool make_heartbeat(uint8_t sequence, p5_can_frame_t *frame) {
  const p5_can_heartbeat_t value = {
      .sequence = sequence,
      .uptime_s = sequence,
      .image_generation_low16 = UINT16_C(0x1234),
  };
  return p5_can_encode_heartbeat(&value, frame) == P5_CAN_RESULT_OK;
}

static bool make_health(uint8_t sequence, p5_can_frame_t *frame) {
  const p5_can_health_summary_t value = {
      .sequence = sequence,
      .health_state = (uint8_t)APP_HEALTH_SERVICEABLE,
      .source_state_pack = 0U,
      .warning_mask = 0U,
  };
  return p5_can_encode_health_summary(&value, frame) == P5_CAN_RESULT_OK;
}

static bool make_event(uint8_t sequence, uint16_t code,
                       p5_can_frame_t *frame) {
  const p5_can_status_event_t value = {
      .sequence = sequence,
      .event_code = code,
      .severity = 1U,
      .source = APP_HEALTH_EVENT_SOURCE,
      .detail = code,
  };
  return p5_can_encode_status_event(&value, frame) == P5_CAN_RESULT_OK;
}

static bool make_climate(uint8_t sequence, p5_can_frame_t *primary,
                         p5_can_frame_t *secondary) {
  const p5_can_climate_primary_t first = {
      .sequence = sequence,
      .temperature_centi_c = 2500,
      .pressure_pa = 101325U,
      .data_flags = 0U,
  };
  const p5_can_climate_secondary_t second = {
      .sequence = sequence,
      .humidity_milli_pct = 50000U,
      .age_100ms = 0U,
  };
  return (p5_can_encode_climate_primary(&first, primary) == P5_CAN_RESULT_OK) &&
         (p5_can_encode_climate_secondary(&second, secondary) ==
          P5_CAN_RESULT_OK);
}

static uint32_t drain_can(app_can_tx_scheduler_t *scheduler) {
  uint32_t committed = 0U;
  p5_can_frame_t frame;
  app_can_tx_token_t token;
  while (app_can_tx_peek(scheduler, &frame, &token)) {
    if (!app_can_tx_commit(scheduler, token)) {
      return UINT32_MAX;
    }
    ++committed;
  }
  return committed;
}

static int d01(void) {
  static const char id[] = "D01";
  harness_t h;
  p5_can_frame_t heartbeat;
  p5_can_frame_t health;
  CHECK(id, initialize(&h));
  CHECK(id, feed_request(&h, 10U, false));
  CHECK(id, feed_request(&h, 500U, false));
  CHECK(id, make_heartbeat(1U, &heartbeat));
  CHECK(id, make_health(1U, &health));
  CHECK(id, app_can_tx_publish_telemetry(
                &h.can_tx, APP_CAN_TELEMETRY_HEARTBEAT, &heartbeat));
  CHECK(id, app_can_tx_publish_telemetry(
                &h.can_tx, APP_CAN_TELEMETRY_HEALTH, &health));
  CHECK(id, drain_can(&h.can_tx) == 2U);
  CHECK(id, advance_tasks(&h) && advance_tasks(&h));
  CHECK(id, evaluate_health(&h, 0U, 0U));
  CHECK(id, h.modbus.accepted == 2U);
  CHECK(id, h.health_decision.state == APP_HEALTH_SERVICEABLE);
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index) {
    CHECK(id, h.tasks[index].release_count == 2U);
  }
  (void)printf("D01 PASS modbus=2 can_tx=2 task_min=2\n");
  return EXIT_SUCCESS;
}

static int d02(void) {
  static const char id[] = "D02";
  harness_t h;
  uint8_t frame[P5_MODBUS_RTU_MAX_ADU_SIZE];
  p5_can_frame_t heartbeat;
  CHECK(id, initialize(&h));
  CHECK(id, feed_request(&h, 10U, false));
  CHECK(id, feed_request(&h, 1000U, false));
  const size_t length = make_request(frame);
  CHECK(id, length == 8U);
  uint32_t end = 2000U;
  p5_modbus_rtu_stream_push_byte(&h.stream, frame[0], end);
  end += CHARACTER_TICKS;
  p5_modbus_rtu_stream_push_byte(&h.stream, frame[1], end);
  end += CHARACTER_TICKS + T1_5_TICKS + 1U;
  for (size_t index = 2U; index < length; ++index) {
    p5_modbus_rtu_stream_push_byte(&h.stream, frame[index], end);
    end += CHARACTER_TICKS;
  }
  p5_modbus_rtu_stream_poll(&h.stream,
                            end - CHARACTER_TICKS + T3_5_TICKS);
  CHECK(id, feed_request(&h, 4000U, false));
  CHECK(id, make_heartbeat(2U, &heartbeat));
  CHECK(id, app_can_tx_publish_telemetry(
                &h.can_tx, APP_CAN_TELEMETRY_HEARTBEAT, &heartbeat));
  CHECK(id, drain_can(&h.can_tx) == 1U);
  CHECK(id, advance_tasks(&h));
  const p5_modbus_rtu_stream_counters_t counters =
      p5_modbus_rtu_stream_counters(&h.stream);
  CHECK(id, h.modbus.accepted == 3U);
  CHECK(id, counters.inter_character_errors == 1U);
  CHECK(id, h.tasks[APP_TASK_CAN].release_count == 1U);
  CHECK(id, h.tasks[APP_TASK_ACQUISITION].release_count == 1U);
  (void)printf("D02 PASS accepted=3 inter_char=1 can_progress=1\n");
  return EXIT_SUCCESS;
}

static int d03(void) {
  static const char id[] = "D03";
  harness_t h;
  p5_can_frame_t heartbeat;
  CHECK(id, initialize(&h));
  CHECK(id, feed_request(&h, 10U, true));
  CHECK(id, feed_request(&h, 500U, false));
  CHECK(id, make_heartbeat(3U, &heartbeat));
  CHECK(id, app_can_tx_publish_telemetry(
                &h.can_tx, APP_CAN_TELEMETRY_HEARTBEAT, &heartbeat));
  CHECK(id, drain_can(&h.can_tx) == 1U);
  CHECK(id, advance_tasks(&h));
  const p5_modbus_rtu_stream_counters_t counters =
      p5_modbus_rtu_stream_counters(&h.stream);
  CHECK(id, counters.crc_mismatches == 1U);
  CHECK(id, h.modbus.accepted == 1U);
  CHECK(id, h.tasks[APP_TASK_HEALTH].release_count == 1U);
  (void)printf("D03 PASS crc=1 recovered_frames=1\n");
  return EXIT_SUCCESS;
}

static int d04(void) {
  static const char id[] = "D04";
  harness_t h;
  const uint8_t response[8] = {4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U};
  p5_can_frame_t heartbeat;
  p5_can_frame_t first;
  p5_can_frame_t retry;
  app_can_tx_token_t first_token;
  app_can_tx_token_t retry_token;
  CHECK(id, initialize(&h));
  CHECK(id, bsp_rs485_state_send(&h.rs485, response, sizeof(response), 100U) ==
                BSP_RS485_RESULT_OK);
  CHECK(id, bsp_rs485_state_send(&h.rs485, response, sizeof(response), 101U) ==
                BSP_RS485_RESULT_BUSY);
  CHECK(id, make_heartbeat(4U, &heartbeat));
  CHECK(id, app_can_tx_publish_telemetry(
                &h.can_tx, APP_CAN_TELEMETRY_HEARTBEAT, &heartbeat));
  CHECK(id, app_can_tx_peek(&h.can_tx, &first, &first_token));
  app_can_tx_note_hal_busy(&h.can_tx);
  CHECK(id, app_can_tx_peek(&h.can_tx, &retry, &retry_token));
  CHECK(id, memcmp(&first, &retry, sizeof(first)) == 0);
  CHECK(id, app_can_tx_commit(&h.can_tx, retry_token));
  const uint32_t timeout = h.rs485.tx_timeout_ms;
  CHECK(id, bsp_rs485_state_poll(&h.rs485, 100U + timeout - 1U) ==
                BSP_RS485_RESULT_OK);
  CHECK(id, bsp_rs485_state_poll(&h.rs485, 100U + timeout) ==
                BSP_RS485_RESULT_TIMEOUT);
  CHECK(id, h.rs485.state == BSP_RS485_LINK_IDLE_RX);
  CHECK(id, advance_tasks(&h));
  CHECK(id, evaluate_health(&h, 0U, 0U));
  CHECK(id, (h.health_decision.warning_mask &
             APP_HEALTH_WARNING_RS485_ERROR) != 0U);
  CHECK(id, h.tasks[APP_TASK_CAN].release_count == 1U);
  (void)printf("D04 PASS busy=1 timeout_ms=%" PRIu32 " can_hal_busy=1\n",
               timeout);
  return EXIT_SUCCESS;
}

static int d05(void) {
  static const char id[] = "D05";
  harness_t h;
  p5_can_frame_t frame;
  p5_can_frame_t selected;
  p5_can_frame_t retry;
  app_can_tx_token_t token;
  app_can_tx_token_t retry_token;
  CHECK(id, initialize(&h));
  CHECK(id, feed_request(&h, 10U, false));
  CHECK(id, make_heartbeat(1U, &frame));
  CHECK(id, app_can_tx_publish_telemetry(
                &h.can_tx, APP_CAN_TELEMETRY_HEARTBEAT, &frame));
  CHECK(id, make_heartbeat(2U, &frame));
  CHECK(id, app_can_tx_publish_telemetry(
                &h.can_tx, APP_CAN_TELEMETRY_HEARTBEAT, &frame));
  CHECK(id, app_can_tx_peek(&h.can_tx, &selected, &token));
  CHECK(id, selected.data[1] == 2U);
  app_can_tx_note_hal_busy(&h.can_tx);
  CHECK(id, app_can_tx_peek(&h.can_tx, &retry, &retry_token));
  CHECK(id, memcmp(&selected, &retry, sizeof(selected)) == 0);
  CHECK(id, app_can_tx_commit(&h.can_tx, retry_token));
  CHECK(id, advance_tasks(&h));
  const app_can_tx_counters_t counters = app_can_tx_counters(&h.can_tx);
  CHECK(id, counters.telemetry_replaced == 1U);
  CHECK(id, counters.hal_busy == 1U);
  CHECK(id, counters.maximum_pending == 1U);
  CHECK(id, h.modbus.accepted == 1U);
  (void)printf("D05 PASS replaced=1 hal_busy=1 max_pending=1\n");
  return EXIT_SUCCESS;
}

static int d06(void) {
  static const char id[] = "D06";
  harness_t h;
  CHECK(id, initialize(&h));
  const app_transport_event_t diagnostic = {
      .timestamp_ms = 1U,
      .detail = 1U,
      .source = APP_HEALTH_EVENT_SOURCE,
      .code = APP_HEALTH_EVENT_CODE_DEGRADED,
  };
  for (uint16_t index = 0U; index < APP_CAN_EVENT_FIFO_DEPTH; ++index) {
    p5_can_frame_t event;
    const uint16_t code = (uint16_t)(UINT16_C(0x0200) + index);
    CHECK(id, make_event((uint8_t)index, code, &event));
    CHECK(id, app_can_tx_enqueue_event(
                  &h.can_tx, code, APP_HEALTH_EVENT_SOURCE, &event));
    CHECK(id, app_transport_event_admit(&h.transport, &diagnostic, true) ==
                  APP_TRANSPORT_EVENT_ACCEPTED);
  }
  p5_can_frame_t overflow;
  CHECK(id, make_event(9U, UINT16_C(0x0300), &overflow));
  CHECK(id, !app_can_tx_enqueue_event(&h.can_tx, UINT16_C(0x0300),
                                      APP_HEALTH_EVENT_SOURCE, &overflow));
  CHECK(id, app_transport_event_admit(&h.transport, &diagnostic, false) ==
                APP_TRANSPORT_EVENT_DROPPED_FULL);

  p5_can_frame_t health;
  p5_can_frame_t primary;
  p5_can_frame_t secondary;
  CHECK(id, make_health(10U, &health));
  CHECK(id, make_climate(10U, &primary, &secondary));
  CHECK(id, app_can_tx_publish_telemetry(
                &h.can_tx, APP_CAN_TELEMETRY_HEALTH, &health));
  CHECK(id, app_can_tx_publish_climate_pair(&h.can_tx, &primary, &secondary));

  bool health_seen = false;
  bool primary_seen = false;
  bool secondary_seen = false;
  p5_can_frame_t sent_primary = {0U};
  p5_can_frame_t sent_secondary = {0U};
  uint32_t committed = 0U;
  p5_can_frame_t selected;
  app_can_tx_token_t token;
  while (app_can_tx_peek(&h.can_tx, &selected, &token)) {
    if (selected.standard_id == P5_CAN_ID_HEALTH_SUMMARY) {
      health_seen = true;
    } else if (selected.standard_id == P5_CAN_ID_CLIMATE_PRIMARY) {
      sent_primary = selected;
      primary_seen = true;
    } else if (selected.standard_id == P5_CAN_ID_CLIMATE_SECONDARY) {
      sent_secondary = selected;
      secondary_seen = true;
    }
    CHECK(id, app_can_tx_commit(&h.can_tx, token));
    ++committed;
  }
  const size_t drained =
      app_transport_event_drain_count(APP_TRANSPORT_EVENT_QUEUE_DEPTH);
  app_transport_note_events_drained(&h.transport, drained);
  CHECK(id, advance_tasks(&h));
  CHECK(id, evaluate_health(&h, APP_TRANSPORT_EVENT_QUEUE_DEPTH,
                            APP_TRANSPORT_EVENT_QUEUE_DEPTH));
  const app_can_tx_counters_t counters = app_can_tx_counters(&h.can_tx);
  CHECK(id, counters.event_dropped == 1U);
  CHECK(id, h.transport.event_dropped_full_count == 1U);
  CHECK(id, drained == APP_TRANSPORT_EVENT_DRAIN_BUDGET);
  CHECK(id, health_seen && primary_seen && secondary_seen);
  CHECK(id, p5_can_climate_pair_matches(&sent_primary, &sent_secondary));
  CHECK(id, counters.maximum_pending <=
                (APP_CAN_EVENT_FIFO_DEPTH + APP_CAN_PERIODIC_FRAME_COUNT));
  CHECK(id, (h.health_decision.warning_mask &
             APP_HEALTH_WARNING_QUEUE_PRESSURE) != 0U);
  (void)printf("D06 PASS committed=%" PRIu32
               " event_drop=1 diag_drop=1 max_pending=%" PRIu32 "\n",
               committed, counters.maximum_pending);
  return EXIT_SUCCESS;
}

static int d07(void) {
  static const char id[] = "D07";
  harness_t h;
  p5_can_frame_t heartbeat;
  CHECK(id, initialize(&h));
  CHECK(id, make_heartbeat(7U, &heartbeat));
  CHECK(id, app_can_tx_publish_telemetry(
                &h.can_tx, APP_CAN_TELEMETRY_HEARTBEAT, &heartbeat));
  CHECK(id, app_can_controller_on_error(
                &h.can_controller, APP_CAN_ERROR_BUS_OFF | APP_CAN_ERROR_ACK,
                10U));
  for (uint32_t attempt = 0U; attempt < APP_CAN_RECOVERY_ATTEMPT_LIMIT;
       ++attempt) {
    CHECK(id, feed_request(&h, 1000U + (attempt * 1000U), false));
    CHECK(id, advance_tasks(&h));
    const uint32_t deadline = h.can_controller.recovery_deadline_ms;
    CHECK(id, !app_can_controller_recovery_due(&h.can_controller,
                                                deadline - 1U));
    CHECK(id, app_can_controller_begin_recovery(&h.can_controller, deadline));
    CHECK(id, app_can_controller_complete_start(&h.can_controller, false,
                                                 deadline));
  }
  CHECK(id, h.can_controller.state == APP_CAN_CONTROLLER_RECOVERY_LATCHED);
  CHECK(id, h.can_controller.counters.recovery_attempts ==
                APP_CAN_RECOVERY_ATTEMPT_LIMIT);
  CHECK(id, h.modbus.accepted == APP_CAN_RECOVERY_ATTEMPT_LIMIT);
  CHECK(id, h.tasks[APP_TASK_ACQUISITION].release_count ==
                APP_CAN_RECOVERY_ATTEMPT_LIMIT);
  CHECK(id, h.tasks[APP_TASK_HEALTH].release_count ==
                APP_CAN_RECOVERY_ATTEMPT_LIMIT);
  CHECK(id, app_can_tx_pending(&h.can_tx) == 1U);
  (void)printf("D07 PASS bus_off=1 attempts=3 modbus_progress=3 "
               "task_progress=3\n");
  return EXIT_SUCCESS;
}

static int d08(void) {
  static const char id[] = "D08";
  harness_t h;
  const uint8_t response[8] = {4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U};
  CHECK(id, initialize(&h));
  CHECK(id, feed_request(&h, 10U, true));
  CHECK(id, bsp_rs485_state_send(&h.rs485, response, sizeof(response), 100U) ==
                BSP_RS485_RESULT_OK);
  CHECK(id, app_can_controller_on_error(
                &h.can_controller, APP_CAN_ERROR_BUS_OFF | APP_CAN_ERROR_ACK,
                100U));
  CHECK(id, bsp_rs485_state_poll(&h.rs485,
                                  100U + h.rs485.tx_timeout_ms) ==
                BSP_RS485_RESULT_TIMEOUT);
  CHECK(id, advance_tasks(&h));
  const uint32_t deadline = h.can_controller.recovery_deadline_ms;
  CHECK(id, app_can_controller_begin_recovery(&h.can_controller, deadline));
  CHECK(id,
        app_can_controller_complete_start(&h.can_controller, true, deadline));
  CHECK(id, feed_request(&h, 2000U, false));
  CHECK(id, bsp_rs485_state_send(&h.rs485, response, sizeof(response), 3000U) ==
                BSP_RS485_RESULT_OK);
  bsp_rs485_state_on_tx_complete(&h.rs485);
  CHECK(id, advance_tasks(&h));
  CHECK(id, evaluate_health(&h, 0U, 0U));
  const p5_modbus_rtu_stream_counters_t counters =
      p5_modbus_rtu_stream_counters(&h.stream);
  CHECK(id, counters.crc_mismatches == 1U);
  CHECK(id, h.rs485.counters.tx_timeouts == 1U);
  CHECK(id, h.rs485.counters.tx_completed == 1U);
  CHECK(id, h.can_controller.counters.bus_off_transitions == 1U);
  CHECK(id, h.can_controller.counters.recovery_attempts == 1U);
  CHECK(id, h.can_controller.state == APP_CAN_CONTROLLER_ACTIVE);
  CHECK(id, h.rs485.state == BSP_RS485_LINK_IDLE_RX);
  CHECK(id, h.modbus.accepted == 1U);
  CHECK(id, h.tasks[APP_TASK_CAN].release_count == 2U);
  CHECK(id, h.tasks[APP_TASK_ACQUISITION].release_count == 2U);
  CHECK(id, (h.health_decision.warning_mask &
             APP_HEALTH_WARNING_RS485_ERROR) != 0U);
  (void)printf("D08 PASS crc=1 rs485_timeout=1 can_recovery=1 both_tasks=2\n");
  return EXIT_SUCCESS;
}

int main(void) {
  CHECK("D01", d01() == EXIT_SUCCESS);
  CHECK("D02", d02() == EXIT_SUCCESS);
  CHECK("D03", d03() == EXIT_SUCCESS);
  CHECK("D04", d04() == EXIT_SUCCESS);
  CHECK("D05", d05() == EXIT_SUCCESS);
  CHECK("D06", d06() == EXIT_SUCCESS);
  CHECK("D07", d07() == EXIT_SUCCESS);
  CHECK("D08", d08() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
