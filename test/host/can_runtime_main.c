#include "app_can_runtime.h"
#include "bsp_can_irq_event.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      return EXIT_FAILURE;                                                     \
    }                                                                          \
  } while (false)

static bsp_can_rx_frame_t make_frame(uint16_t standard_id, uint8_t sequence) {
  bsp_can_rx_frame_t frame = {
      .standard_id = standard_id,
      .ide = BSP_CAN_IDE_STANDARD,
      .rtr = BSP_CAN_RTR_DATA,
      .dlc = P5_CAN_DLC,
      .data = {0U},
  };
  frame.data[0] = sequence;
  return frame;
}

static int test_exact_filter_plan(void) {
  static const uint16_t expected_ids[] = {
      P5_CAN_ID_STATUS_EVENT,      P5_CAN_ID_HEARTBEAT,
      P5_CAN_ID_HEALTH_SUMMARY,    P5_CAN_ID_CLIMATE_PRIMARY,
      P5_CAN_ID_CLIMATE_SECONDARY, P5_CAN_ID_ILLUMINANCE,
      P5_CAN_ID_VIBRATION_SUMMARY, P5_CAN_ID_VIBRATION_SUMMARY,
  };
  bsp_can_filter_plan_t plan;
  bsp_can_filter_plan_build(&plan);

  for (uint32_t index = 0U;
       index < (sizeof(expected_ids) / sizeof(expected_ids[0])); ++index) {
    uint16_t encoded = 0U;
    CHECK(bsp_can_filter_encode_standard_id(expected_ids[index], &encoded));
    CHECK(encoded == (uint16_t)(expected_ids[index] << 5U));
    CHECK(plan.banks[index / BSP_CAN_FILTER_ENTRIES_PER_BANK]
              .entries[index % BSP_CAN_FILTER_ENTRIES_PER_BANK] == encoded);
  }
  uint16_t encoded = 0U;
  CHECK(!bsp_can_filter_encode_standard_id(UINT16_C(0x0800), &encoded));
  CHECK(!bsp_can_filter_encode_standard_id(P5_CAN_ID_HEARTBEAT, NULL));
  return EXIT_SUCCESS;
}

static int test_header_whitelist(void) {
  static const uint16_t accepted_ids[] = {
      P5_CAN_ID_STATUS_EVENT,      P5_CAN_ID_HEARTBEAT,
      P5_CAN_ID_HEALTH_SUMMARY,    P5_CAN_ID_CLIMATE_PRIMARY,
      P5_CAN_ID_CLIMATE_SECONDARY, P5_CAN_ID_ILLUMINANCE,
      P5_CAN_ID_VIBRATION_SUMMARY,
  };
  for (uint32_t index = 0U;
       index < (sizeof(accepted_ids) / sizeof(accepted_ids[0])); ++index) {
    CHECK(bsp_can_header_is_accepted(accepted_ids[index], BSP_CAN_IDE_STANDARD,
                                     BSP_CAN_RTR_DATA, P5_CAN_DLC));
  }
  CHECK(!bsp_can_header_is_accepted(UINT32_C(0x13f), 0U, 0U, 8U));
  CHECK(!bsp_can_header_is_accepted(UINT32_C(0x141), 0U, 0U, 8U));
  CHECK(!bsp_can_header_is_accepted(P5_CAN_ID_HEARTBEAT, 1U, 0U, 8U));
  CHECK(!bsp_can_header_is_accepted(P5_CAN_ID_HEARTBEAT, 0U, 1U, 8U));
  CHECK(!bsp_can_header_is_accepted(P5_CAN_ID_HEARTBEAT, 0U, 0U, 7U));
  return EXIT_SUCCESS;
}

static int test_rx_ring_wrap_and_take(void) {
  bsp_can_irq_mailbox_t mailbox;
  bsp_can_irq_event_snapshot_t snapshot;
  bsp_can_irq_mailbox_initialize(&mailbox);

  for (uint8_t sequence = 0U; sequence < 4U; ++sequence) {
    const bsp_can_rx_frame_t frame = make_frame(P5_CAN_ID_HEARTBEAT, sequence);
    CHECK(bsp_can_irq_mailbox_publish_rx(&mailbox, &frame) ==
          BSP_CAN_IRQ_EVENT_RX_READY);
  }
  CHECK(bsp_can_irq_mailbox_take(&mailbox, &snapshot));
  CHECK(snapshot.event_mask == BSP_CAN_IRQ_EVENT_RX_READY);
  CHECK(snapshot.rx_pending == BSP_CAN_RX_RING_CAPACITY);

  for (uint8_t sequence = 0U; sequence < 2U; ++sequence) {
    bsp_can_rx_frame_t frame;
    CHECK(bsp_can_irq_mailbox_pop_rx(&mailbox, &frame));
    CHECK(frame.data[0] == sequence);
  }
  for (uint8_t sequence = 4U; sequence < 6U; ++sequence) {
    const bsp_can_rx_frame_t frame = make_frame(P5_CAN_ID_HEARTBEAT, sequence);
    CHECK(bsp_can_irq_mailbox_publish_rx(&mailbox, &frame) != 0U);
  }
  for (uint8_t sequence = 2U; sequence < 6U; ++sequence) {
    bsp_can_rx_frame_t frame;
    CHECK(bsp_can_irq_mailbox_pop_rx(&mailbox, &frame));
    CHECK(frame.data[0] == sequence);
  }
  bsp_can_rx_frame_t empty;
  CHECK(!bsp_can_irq_mailbox_pop_rx(&mailbox, &empty));
  CHECK(!bsp_can_irq_mailbox_take(&mailbox, &snapshot));
  CHECK(bsp_can_irq_mailbox_counters(&mailbox).rx_accepted == 6U);
  return EXIT_SUCCESS;
}

static int test_rx_full_invalid_and_saturation(void) {
  bsp_can_irq_mailbox_t mailbox;
  bsp_can_irq_mailbox_initialize(&mailbox);
  const bsp_can_rx_frame_t valid = make_frame(P5_CAN_ID_HEALTH_SUMMARY, 1U);
  for (uint32_t index = 0U; index < BSP_CAN_RX_RING_CAPACITY; ++index) {
    CHECK(bsp_can_irq_mailbox_publish_rx(&mailbox, &valid) != 0U);
  }
  CHECK(bsp_can_irq_mailbox_publish_rx(&mailbox, &valid) == 0U);
  CHECK(bsp_can_irq_mailbox_counters(&mailbox).rx_dropped == 1U);

  bsp_can_rx_frame_t invalid = valid;
  invalid.standard_id = UINT32_C(0x141);
  CHECK(bsp_can_irq_mailbox_publish_rx(&mailbox, &invalid) == 0U);
  invalid = valid;
  invalid.ide = 1U;
  CHECK(bsp_can_irq_mailbox_publish_rx(&mailbox, &invalid) == 0U);
  invalid = valid;
  invalid.rtr = 1U;
  CHECK(bsp_can_irq_mailbox_publish_rx(&mailbox, &invalid) == 0U);
  invalid = valid;
  invalid.dlc = 7U;
  CHECK(bsp_can_irq_mailbox_publish_rx(&mailbox, &invalid) == 0U);
  CHECK(bsp_can_irq_mailbox_counters(&mailbox).invalid_headers == 4U);

  mailbox.counters.rx_dropped = UINT32_MAX;
  CHECK(bsp_can_irq_mailbox_publish_rx(&mailbox, &valid) == 0U);
  CHECK(mailbox.counters.rx_dropped == UINT32_MAX);
  return EXIT_SUCCESS;
}

static int test_event_merge_and_latest_error(void) {
  bsp_can_irq_mailbox_t mailbox;
  bsp_can_irq_event_snapshot_t snapshot;
  bsp_can_irq_mailbox_initialize(&mailbox);

  CHECK(bsp_can_irq_mailbox_publish_tx_complete(&mailbox, 0U) != 0U);
  CHECK(bsp_can_irq_mailbox_publish_tx_complete(&mailbox, 2U) != 0U);
  CHECK(bsp_can_irq_mailbox_publish_tx_complete(&mailbox, 2U) != 0U);
  CHECK(bsp_can_irq_mailbox_publish_tx_abort(&mailbox, 1U) != 0U);
  CHECK(bsp_can_irq_mailbox_publish_error(&mailbox, 0x10U, 2U) != 0U);
  CHECK(bsp_can_irq_mailbox_publish_error(&mailbox, 0x20U, 3U) != 0U);
  CHECK(bsp_can_irq_mailbox_publish_tx_complete(&mailbox, 3U) == 0U);

  bsp_can_irq_mailbox_note_deferred(&mailbox);
  CHECK(bsp_can_irq_mailbox_take(&mailbox, &snapshot));
  CHECK(snapshot.event_mask ==
        (BSP_CAN_IRQ_EVENT_TX_COMPLETE | BSP_CAN_IRQ_EVENT_TX_ABORT |
         BSP_CAN_IRQ_EVENT_ERROR));
  CHECK(snapshot.tx_complete_mask == UINT8_C(0x05));
  CHECK(snapshot.tx_abort_mask == UINT8_C(0x02));
  CHECK(snapshot.latest_hal_error == 0x20U);
  CHECK(snapshot.latest_controller_state == 3U);
  CHECK(!bsp_can_irq_mailbox_take(&mailbox, &snapshot));

  const bsp_can_irq_event_counters_t counters =
      bsp_can_irq_mailbox_counters(&mailbox);
  CHECK(counters.tx_completed == 3U);
  CHECK(counters.tx_aborted == 1U);
  CHECK(counters.error_callbacks == 2U);
  CHECK(counters.published_events == 6U);
  CHECK(counters.coalesced_events == 3U);
  CHECK(counters.deferred_notifications == 1U);
  CHECK(counters.snapshots_taken == 1U);
  return EXIT_SUCCESS;
}

static int test_counter_saturation(void) {
  bsp_can_irq_mailbox_t mailbox;
  bsp_can_irq_mailbox_initialize(&mailbox);
  mailbox.counters.published_events = UINT32_MAX;
  mailbox.counters.coalesced_events = UINT32_MAX;
  mailbox.counters.tx_completed = UINT32_MAX;
  mailbox.counters.tx_aborted = UINT32_MAX;
  mailbox.counters.error_callbacks = UINT32_MAX;
  mailbox.counters.deferred_notifications = UINT32_MAX;

  CHECK(bsp_can_irq_mailbox_publish_tx_complete(&mailbox, 0U) != 0U);
  CHECK(bsp_can_irq_mailbox_publish_tx_complete(&mailbox, 1U) != 0U);
  CHECK(bsp_can_irq_mailbox_publish_tx_abort(&mailbox, 0U) != 0U);
  CHECK(bsp_can_irq_mailbox_publish_error(&mailbox, 1U, 1U) != 0U);
  bsp_can_irq_mailbox_note_deferred(&mailbox);
  const bsp_can_irq_event_counters_t counters =
      bsp_can_irq_mailbox_counters(&mailbox);
  CHECK(counters.published_events == UINT32_MAX);
  CHECK(counters.coalesced_events == UINT32_MAX);
  CHECK(counters.tx_completed == UINT32_MAX);
  CHECK(counters.tx_aborted == UINT32_MAX);
  CHECK(counters.error_callbacks == UINT32_MAX);
  CHECK(counters.deferred_notifications == UINT32_MAX);
  return EXIT_SUCCESS;
}

static p5_can_frame_t make_status_event(uint8_t sequence, uint16_t event_code,
                                        uint8_t source, uint16_t detail) {
  const p5_can_status_event_t payload = {
      .sequence = sequence,
      .event_code = event_code,
      .severity = 1U,
      .source = source,
      .detail = detail,
  };
  p5_can_frame_t frame;
  if (p5_can_encode_status_event(&payload, &frame) != P5_CAN_RESULT_OK) {
    abort();
  }
  return frame;
}

static p5_can_frame_t make_heartbeat(uint8_t sequence) {
  const p5_can_heartbeat_t payload = {
      .sequence = sequence,
      .uptime_s = sequence,
      .image_generation_low16 = UINT16_C(0x1234),
  };
  p5_can_frame_t frame;
  if (p5_can_encode_heartbeat(&payload, &frame) != P5_CAN_RESULT_OK) {
    abort();
  }
  return frame;
}

static p5_can_frame_t make_health(uint8_t sequence) {
  const p5_can_health_summary_t payload = {
      .sequence = sequence,
      .health_state = 1U,
      .source_state_pack = 0U,
      .warning_mask = 0U,
  };
  p5_can_frame_t frame;
  if (p5_can_encode_health_summary(&payload, &frame) != P5_CAN_RESULT_OK) {
    abort();
  }
  return frame;
}

static p5_can_frame_t make_scalar(uint8_t sequence, bool vibration) {
  const p5_can_scalar_summary_t payload = {
      .sequence = sequence,
      .value = sequence,
      .data_flags = 0U,
      .age_100ms = 0U,
  };
  p5_can_frame_t frame;
  const p5_can_result_t result =
      vibration ? p5_can_encode_vibration_summary(&payload, &frame)
                : p5_can_encode_illuminance(&payload, &frame);
  if (result != P5_CAN_RESULT_OK) {
    abort();
  }
  return frame;
}

static void make_climate_pair(uint8_t sequence, p5_can_frame_t *primary,
                              p5_can_frame_t *secondary) {
  const p5_can_climate_primary_t primary_payload = {
      .sequence = sequence,
      .temperature_centi_c = 2500,
      .pressure_pa = 101325U,
      .data_flags = 0U,
  };
  const p5_can_climate_secondary_t secondary_payload = {
      .sequence = sequence,
      .humidity_milli_pct = 50000U,
      .age_100ms = 0U,
  };
  if ((p5_can_encode_climate_primary(&primary_payload, primary) !=
       P5_CAN_RESULT_OK) ||
      (p5_can_encode_climate_secondary(&secondary_payload, secondary) !=
       P5_CAN_RESULT_OK)) {
    abort();
  }
}

static int test_event_fifo_coalesce_and_full(void) {
  app_can_tx_scheduler_t scheduler;
  app_can_tx_scheduler_initialize(&scheduler);
  for (uint16_t code = 0U; code < APP_CAN_EVENT_FIFO_DEPTH; ++code) {
    const p5_can_frame_t frame =
        make_status_event((uint8_t)code, code, 1U, code);
    CHECK(app_can_tx_enqueue_event(&scheduler, code, 1U, &frame));
  }
  CHECK(app_can_tx_pending(&scheduler) == APP_CAN_EVENT_FIFO_DEPTH);

  p5_can_frame_t replacement = make_status_event(9U, 0U, 1U, 99U);
  CHECK(app_can_tx_enqueue_event(&scheduler, 0U, 1U, &replacement));
  const p5_can_frame_t overflow = make_status_event(10U, 99U, 1U, 1U);
  CHECK(!app_can_tx_enqueue_event(&scheduler, 99U, 1U, &overflow));
  const app_can_tx_counters_t counters = app_can_tx_counters(&scheduler);
  CHECK(counters.event_enqueued == APP_CAN_EVENT_FIFO_DEPTH);
  CHECK(counters.event_coalesced == 1U);
  CHECK(counters.event_dropped == 1U);
  CHECK(counters.maximum_pending == APP_CAN_EVENT_FIFO_DEPTH);

  p5_can_frame_t selected;
  app_can_tx_token_t token;
  CHECK(app_can_tx_peek(&scheduler, &selected, &token));
  CHECK(selected.data[1] == 9U);
  CHECK(app_can_tx_commit(&scheduler, token));
  CHECK(app_can_tx_pending(&scheduler) == (APP_CAN_EVENT_FIFO_DEPTH - 1U));
  return EXIT_SUCCESS;
}

static int test_telemetry_latest_and_climate_atomicity(void) {
  app_can_tx_scheduler_t scheduler;
  app_can_tx_scheduler_initialize(&scheduler);
  p5_can_frame_t heartbeat = make_heartbeat(1U);
  CHECK(app_can_tx_publish_telemetry(&scheduler, APP_CAN_TELEMETRY_HEARTBEAT,
                                     &heartbeat));
  heartbeat = make_heartbeat(2U);
  CHECK(app_can_tx_publish_telemetry(&scheduler, APP_CAN_TELEMETRY_HEARTBEAT,
                                     &heartbeat));

  p5_can_frame_t selected;
  app_can_tx_token_t token;
  CHECK(app_can_tx_peek(&scheduler, &selected, &token));
  CHECK(selected.standard_id == P5_CAN_ID_HEARTBEAT);
  CHECK(selected.data[1] == 2U);
  CHECK(app_can_tx_commit(&scheduler, token));

  p5_can_frame_t primary;
  p5_can_frame_t secondary;
  make_climate_pair(10U, &primary, &secondary);
  CHECK(app_can_tx_publish_climate_pair(&scheduler, &primary, &secondary));
  CHECK(app_can_tx_peek(&scheduler, &selected, &token));
  CHECK(selected.standard_id == P5_CAN_ID_CLIMATE_PRIMARY);
  CHECK(app_can_tx_commit(&scheduler, token));

  p5_can_frame_t next_primary;
  p5_can_frame_t next_secondary;
  make_climate_pair(11U, &next_primary, &next_secondary);
  CHECK(!app_can_tx_publish_climate_pair(&scheduler, &next_primary,
                                         &next_secondary));
  CHECK(app_can_tx_peek(&scheduler, &selected, &token));
  CHECK(selected.standard_id == P5_CAN_ID_CLIMATE_SECONDARY);
  CHECK(selected.data[1] == 10U);
  CHECK(app_can_tx_commit(&scheduler, token));
  CHECK(app_can_tx_publish_climate_pair(&scheduler, &next_primary,
                                        &next_secondary));

  const app_can_tx_counters_t counters = app_can_tx_counters(&scheduler);
  CHECK(counters.telemetry_replaced == 1U);
  CHECK(counters.telemetry_dropped == 2U);
  return EXIT_SUCCESS;
}

static int test_busy_preserves_head_and_event_fairness(void) {
  app_can_tx_scheduler_t scheduler;
  app_can_tx_scheduler_initialize(&scheduler);
  for (uint16_t code = 1U; code <= 4U; ++code) {
    const p5_can_frame_t frame =
        make_status_event((uint8_t)code, code, 1U, code);
    CHECK(app_can_tx_enqueue_event(&scheduler, code, 1U, &frame));
  }
  const p5_can_frame_t light = make_scalar(7U, false);
  CHECK(app_can_tx_publish_telemetry(&scheduler, APP_CAN_TELEMETRY_ILLUMINANCE,
                                     &light));

  p5_can_frame_t first;
  p5_can_frame_t retry;
  app_can_tx_token_t token;
  app_can_tx_token_t retry_token;
  CHECK(app_can_tx_peek(&scheduler, &first, &token));
  app_can_tx_note_hal_busy(&scheduler);
  CHECK(app_can_tx_peek(&scheduler, &retry, &retry_token));
  CHECK(first.standard_id == retry.standard_id);
  CHECK(first.data[1] == retry.data[1]);
  CHECK(token.kind == retry_token.kind);
  CHECK(app_can_tx_commit(&scheduler, retry_token));

  CHECK(app_can_tx_peek(&scheduler, &first, &token));
  CHECK(first.standard_id == P5_CAN_ID_STATUS_EVENT);
  CHECK(app_can_tx_commit(&scheduler, token));
  CHECK(app_can_tx_peek(&scheduler, &first, &token));
  CHECK(first.standard_id == P5_CAN_ID_ILLUMINANCE);
  CHECK(app_can_tx_commit(&scheduler, token));
  CHECK(app_can_tx_counters(&scheduler).hal_busy == 1U);
  return EXIT_SUCCESS;
}

static int test_periodic_round_robin(void) {
  app_can_tx_scheduler_t scheduler;
  app_can_tx_scheduler_initialize(&scheduler);
  const p5_can_frame_t heartbeat = make_heartbeat(1U);
  const p5_can_frame_t health = make_health(2U);
  const p5_can_frame_t light = make_scalar(3U, false);
  const p5_can_frame_t vibration = make_scalar(4U, true);
  CHECK(app_can_tx_publish_telemetry(&scheduler, APP_CAN_TELEMETRY_HEARTBEAT,
                                     &heartbeat));
  CHECK(app_can_tx_publish_telemetry(&scheduler, APP_CAN_TELEMETRY_HEALTH,
                                     &health));
  CHECK(app_can_tx_publish_telemetry(&scheduler, APP_CAN_TELEMETRY_ILLUMINANCE,
                                     &light));
  CHECK(app_can_tx_publish_telemetry(&scheduler, APP_CAN_TELEMETRY_VIBRATION,
                                     &vibration));

  static const uint16_t expected[] = {
      P5_CAN_ID_HEARTBEAT,
      P5_CAN_ID_HEALTH_SUMMARY,
      P5_CAN_ID_ILLUMINANCE,
      P5_CAN_ID_VIBRATION_SUMMARY,
  };
  for (uint32_t index = 0U; index < (sizeof(expected) / sizeof(expected[0]));
       ++index) {
    p5_can_frame_t frame;
    app_can_tx_token_t token;
    CHECK(app_can_tx_peek(&scheduler, &frame, &token));
    CHECK(frame.standard_id == expected[index]);
    CHECK(app_can_tx_commit(&scheduler, token));
  }
  CHECK(app_can_tx_pending(&scheduler) == 0U);
  return EXIT_SUCCESS;
}

static int test_controller_recovery_limits(void) {
  app_can_controller_t controller;
  app_can_controller_initialize(&controller);
  CHECK(app_can_controller_begin_initial_start(&controller));
  CHECK(app_can_controller_complete_start(&controller, false, 0U));
  CHECK(controller.state == APP_CAN_CONTROLLER_RECOVERY_WAIT);
  CHECK(!app_can_controller_recovery_due(&controller, 999U));

  for (uint32_t attempt = 1U; attempt <= APP_CAN_RECOVERY_ATTEMPT_LIMIT;
       ++attempt) {
    const uint32_t now_ms = attempt * APP_CAN_RECOVERY_DELAY_MS;
    CHECK(app_can_controller_recovery_due(&controller, now_ms));
    CHECK(app_can_controller_begin_recovery(&controller, now_ms));
    CHECK(app_can_controller_complete_start(&controller, false, now_ms));
  }
  CHECK(controller.state == APP_CAN_CONTROLLER_RECOVERY_LATCHED);
  CHECK(controller.recovery_attempts_in_episode ==
        APP_CAN_RECOVERY_ATTEMPT_LIMIT);
  CHECK(controller.counters.recovery_attempts ==
        APP_CAN_RECOVERY_ATTEMPT_LIMIT);
  CHECK(!app_can_controller_begin_recovery(&controller, 10000U));
  return EXIT_SUCCESS;
}

static int test_controller_error_classification_and_wrap(void) {
  app_can_controller_t controller;
  app_can_controller_initialize(&controller);
  CHECK(app_can_controller_begin_initial_start(&controller));
  CHECK(app_can_controller_complete_start(&controller, true, 0U));
  CHECK(!app_can_controller_on_error(&controller,
                                     APP_CAN_ERROR_ARBITRATION_LOST, 1U));
  CHECK(controller.state == APP_CAN_CONTROLLER_ACTIVE);
  CHECK(app_can_controller_on_error(
      &controller, APP_CAN_ERROR_WARNING | APP_CAN_ERROR_ACK, 2U));
  CHECK(controller.state == APP_CAN_CONTROLLER_WARNING);
  CHECK(app_can_controller_on_error(
      &controller, APP_CAN_ERROR_PASSIVE | APP_CAN_ERROR_TX, 3U));
  CHECK(controller.state == APP_CAN_CONTROLLER_PASSIVE);

  const uint32_t bus_off_ms = UINT32_C(0xffffff00);
  CHECK(app_can_controller_on_error(
      &controller, APP_CAN_ERROR_BUS_OFF | APP_CAN_ERROR_RX_OVERRUN,
      bus_off_ms));
  CHECK(controller.state == APP_CAN_CONTROLLER_BUS_OFF);
  const uint32_t recovery_deadline = controller.recovery_deadline_ms;
  CHECK(!app_can_controller_on_error(&controller, APP_CAN_ERROR_BUS_OFF,
                                     bus_off_ms + 100U));
  CHECK(controller.recovery_deadline_ms == recovery_deadline);
  CHECK(!app_can_controller_on_error(&controller, APP_CAN_ERROR_WARNING,
                                     bus_off_ms + 200U));
  CHECK(controller.state == APP_CAN_CONTROLLER_BUS_OFF);
  CHECK(!app_can_controller_recovery_due(&controller,
                                         controller.recovery_deadline_ms - 1U));
  CHECK(app_can_controller_recovery_due(&controller,
                                        controller.recovery_deadline_ms));
  CHECK(app_can_controller_begin_recovery(&controller,
                                          controller.recovery_deadline_ms));
  CHECK(app_can_controller_complete_start(&controller, true,
                                          controller.recovery_deadline_ms));
  CHECK(controller.state == APP_CAN_CONTROLLER_ACTIVE);
  CHECK(controller.recovery_attempts_in_episode == 0U);
  CHECK(controller.counters.arbitration_lost == 1U);
  CHECK(controller.counters.ack_errors == 1U);
  CHECK(controller.counters.tx_errors == 1U);
  CHECK(controller.counters.rx_overruns == 1U);
  return EXIT_SUCCESS;
}

static int test_periodic_frame_projection(void) {
  app_measurement_snapshot_t measurement = {0};
  measurement.schema_revision = APP_MEASUREMENT_SCHEMA_REVISION;
  measurement.bme280.metadata.state = APP_MEASUREMENT_STATE_FRESH;
  measurement.bme280.metadata.value_present = true;
  measurement.bme280.temperature_centi_c = 2534;
  measurement.bme280.pressure_pa = 101325U;
  measurement.bme280.humidity_milli_pct = 45678U;
  measurement.bme280.metadata.age_ms = 250U;
  measurement.veml7700.metadata.state = APP_MEASUREMENT_STATE_STALE;
  measurement.veml7700.metadata.value_present = true;
  measurement.veml7700.metadata.value_is_retained = true;
  measurement.veml7700.metadata.quality_flags = 1U;
  measurement.veml7700.metadata.age_ms = 390U;
  measurement.veml7700.illuminance_millilux = 123456U;
  measurement.adxl345_sample.metadata.state = APP_MEASUREMENT_STATE_FRESH;
  measurement.adxl345_feature.metadata.state = APP_MEASUREMENT_STATE_FRESH;
  measurement.adxl345_feature.metadata.value_present = true;
  measurement.adxl345_feature.metadata.age_ms = 50U;
  measurement.adxl345_feature.resultant_rms_millig = 789U;
  const app_health_decision_t health = {
      .state = APP_HEALTH_DEGRADED,
      .warning_mask = UINT32_C(0x12345678),
  };
  p5_can_frame_t frames[APP_CAN_PERIODIC_FRAME_COUNT];
  CHECK(app_can_build_periodic_frames(&measurement, &health, 0x12345U, 99U, 7U,
                                      frames));

  p5_can_heartbeat_t heartbeat;
  p5_can_health_summary_t health_summary;
  p5_can_climate_primary_t primary;
  p5_can_climate_secondary_t secondary;
  p5_can_scalar_summary_t light;
  p5_can_scalar_summary_t vibration;
  CHECK(p5_can_decode_heartbeat(&frames[APP_CAN_PERIODIC_HEARTBEAT],
                                &heartbeat) == P5_CAN_RESULT_OK);
  CHECK(p5_can_decode_health_summary(&frames[APP_CAN_PERIODIC_HEALTH],
                                     &health_summary) == P5_CAN_RESULT_OK);
  CHECK(p5_can_decode_climate_primary(&frames[APP_CAN_PERIODIC_CLIMATE_PRIMARY],
                                      &primary) == P5_CAN_RESULT_OK);
  CHECK(p5_can_decode_climate_secondary(
            &frames[APP_CAN_PERIODIC_CLIMATE_SECONDARY], &secondary) ==
        P5_CAN_RESULT_OK);
  CHECK(p5_can_decode_illuminance(&frames[APP_CAN_PERIODIC_ILLUMINANCE],
                                  &light) == P5_CAN_RESULT_OK);
  CHECK(p5_can_decode_vibration_summary(&frames[APP_CAN_PERIODIC_VIBRATION],
                                        &vibration) == P5_CAN_RESULT_OK);
  CHECK(heartbeat.sequence == 7U);
  CHECK(heartbeat.uptime_s == 99U);
  CHECK(heartbeat.image_generation_low16 == UINT16_C(0x2345));
  CHECK(health_summary.health_state == APP_HEALTH_DEGRADED);
  CHECK(health_summary.warning_mask == UINT32_C(0x12345678));
  CHECK(primary.temperature_centi_c == 2534);
  CHECK(primary.pressure_pa == 101325U);
  CHECK(secondary.humidity_milli_pct == 45678U);
  CHECK(secondary.age_100ms == 2U);
  CHECK(light.value == 123456U);
  CHECK((light.data_flags & P5_CAN_DATA_RETAINED) != 0U);
  CHECK((light.data_flags & P5_CAN_DATA_QUALITY_WARNING) != 0U);
  CHECK(light.age_100ms == 3U);
  CHECK(vibration.value == 789U);
  return EXIT_SUCCESS;
}

static int test_periodic_invalid_projection(void) {
  app_measurement_snapshot_t measurement = {0};
  measurement.schema_revision = APP_MEASUREMENT_SCHEMA_REVISION;
  measurement.bme280.metadata.state = APP_MEASUREMENT_STATE_FRESH;
  measurement.bme280.metadata.value_present = true;
  measurement.bme280.temperature_centi_c = INT32_MAX;
  measurement.bme280.pressure_pa = 101325U;
  measurement.bme280.humidity_milli_pct = 50000U;
  measurement.veml7700.metadata.state = APP_MEASUREMENT_STATE_OFFLINE;
  measurement.adxl345_sample.metadata.state = APP_MEASUREMENT_STATE_OFFLINE;
  measurement.adxl345_feature.metadata.state = APP_MEASUREMENT_STATE_OFFLINE;
  const app_health_decision_t health = {.state = APP_HEALTH_BOOTSTRAP};
  p5_can_frame_t frames[APP_CAN_PERIODIC_FRAME_COUNT];
  CHECK(
      app_can_build_periodic_frames(&measurement, &health, 0U, 0U, 0U, frames));

  p5_can_climate_primary_t primary;
  p5_can_climate_secondary_t secondary;
  p5_can_scalar_summary_t light;
  CHECK(p5_can_decode_climate_primary(&frames[APP_CAN_PERIODIC_CLIMATE_PRIMARY],
                                      &primary) == P5_CAN_RESULT_OK);
  CHECK(p5_can_decode_climate_secondary(
            &frames[APP_CAN_PERIODIC_CLIMATE_SECONDARY], &secondary) ==
        P5_CAN_RESULT_OK);
  CHECK(p5_can_decode_illuminance(&frames[APP_CAN_PERIODIC_ILLUMINANCE],
                                  &light) == P5_CAN_RESULT_OK);
  CHECK(primary.temperature_centi_c == P5_CAN_TEMPERATURE_INVALID);
  CHECK(primary.pressure_pa == P5_CAN_PRESSURE_INVALID);
  CHECK((primary.data_flags & P5_CAN_DATA_QUALITY_WARNING) != 0U);
  CHECK(secondary.humidity_milli_pct == P5_CAN_VALUE_U32_INVALID);
  CHECK(light.value == P5_CAN_VALUE_U32_INVALID);
  CHECK(light.age_100ms == P5_CAN_AGE_U8_UNKNOWN);
  return EXIT_SUCCESS;
}

int main(void) {
  CHECK(test_exact_filter_plan() == EXIT_SUCCESS);
  CHECK(test_header_whitelist() == EXIT_SUCCESS);
  CHECK(test_rx_ring_wrap_and_take() == EXIT_SUCCESS);
  CHECK(test_rx_full_invalid_and_saturation() == EXIT_SUCCESS);
  CHECK(test_event_merge_and_latest_error() == EXIT_SUCCESS);
  CHECK(test_counter_saturation() == EXIT_SUCCESS);
  CHECK(test_event_fifo_coalesce_and_full() == EXIT_SUCCESS);
  CHECK(test_telemetry_latest_and_climate_atomicity() == EXIT_SUCCESS);
  CHECK(test_busy_preserves_head_and_event_fairness() == EXIT_SUCCESS);
  CHECK(test_periodic_round_robin() == EXIT_SUCCESS);
  CHECK(test_controller_recovery_limits() == EXIT_SUCCESS);
  CHECK(test_controller_error_classification_and_wrap() == EXIT_SUCCESS);
  CHECK(test_periodic_frame_projection() == EXIT_SUCCESS);
  CHECK(test_periodic_invalid_projection() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
