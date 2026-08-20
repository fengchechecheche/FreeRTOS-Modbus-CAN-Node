#include "app_can_runtime.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint32_t app_can_saturating_increment(uint32_t value) {
  return (value == UINT32_MAX) ? UINT32_MAX : (value + 1U);
}

static uint32_t app_can_saturating_add(uint32_t value, uint32_t increment) {
  return (increment > (UINT32_MAX - value)) ? UINT32_MAX : (value + increment);
}

static bool app_can_time_reached(uint32_t now_ms, uint32_t deadline_ms) {
  return (int32_t)(now_ms - deadline_ms) >= 0;
}

static uint32_t app_can_count_bits(uint8_t value) {
  uint32_t count = 0U;
  while (value != 0U) {
    ++count;
    value &= (uint8_t)(value - 1U);
  }
  return count;
}

static uint8_t app_can_data_flags(
    const app_measurement_metadata_t *metadata, bool value_out_of_range) {
  uint8_t flags = (uint8_t)metadata->state & P5_CAN_DATA_STATE_MASK;
  if (metadata->value_is_retained) {
    flags |= P5_CAN_DATA_RETAINED;
  }
  if ((metadata->quality_flags != 0U) || value_out_of_range) {
    flags |= P5_CAN_DATA_QUALITY_WARNING;
  }
  return flags;
}

static bool app_can_all_encodes_succeeded(const p5_can_result_t results[6]) {
  for (size_t index = 0U; index < APP_CAN_PERIODIC_FRAME_COUNT; ++index) {
    if (results[index] != P5_CAN_RESULT_OK) {
      return false;
    }
  }
  return true;
}

bool app_can_build_periodic_frames(
    const app_measurement_snapshot_t *measurement,
    const app_health_decision_t *health, uint32_t image_generation,
    uint32_t uptime_s, uint8_t sequence,
    p5_can_frame_t frames[APP_CAN_PERIODIC_FRAME_COUNT]) {
  if ((measurement == NULL) || (health == NULL) || (frames == NULL) ||
      (measurement->schema_revision != APP_MEASUREMENT_SCHEMA_REVISION)) {
    return false;
  }

  const uint8_t source_states[4] = {
      (uint8_t)measurement->bme280.metadata.state,
      (uint8_t)measurement->veml7700.metadata.state,
      (uint8_t)measurement->adxl345_sample.metadata.state,
      (uint8_t)measurement->adxl345_feature.metadata.state,
  };
  uint8_t packed_source_states = 0U;
  if (p5_can_pack_source_states(source_states, &packed_source_states) !=
      P5_CAN_RESULT_OK) {
    return false;
  }

  const bool bme_value_in_range =
      measurement->bme280.metadata.value_present &&
      (measurement->bme280.temperature_centi_c >= INT16_MIN) &&
      (measurement->bme280.temperature_centi_c <= INT16_MAX) &&
      (measurement->bme280.pressure_pa < P5_CAN_PRESSURE_INVALID) &&
      (measurement->bme280.humidity_milli_pct <= UINT32_C(100000));
  const bool light_value_present =
      measurement->veml7700.metadata.value_present;
  const bool vibration_value_present =
      measurement->adxl345_feature.metadata.value_present;

  const p5_can_heartbeat_t heartbeat = {
      .sequence = sequence,
      .uptime_s = uptime_s,
      .image_generation_low16 = (uint16_t)image_generation,
  };
  const p5_can_health_summary_t health_summary = {
      .sequence = sequence,
      .health_state = (uint8_t)health->state,
      .source_state_pack = packed_source_states,
      .warning_mask = health->warning_mask,
  };
  const p5_can_climate_primary_t climate_primary = {
      .sequence = sequence,
      .temperature_centi_c = bme_value_in_range
                                 ? (int16_t)measurement->bme280.temperature_centi_c
                                 : P5_CAN_TEMPERATURE_INVALID,
      .pressure_pa = bme_value_in_range ? measurement->bme280.pressure_pa
                                        : P5_CAN_PRESSURE_INVALID,
      .data_flags = app_can_data_flags(
          &measurement->bme280.metadata,
          measurement->bme280.metadata.value_present && !bme_value_in_range),
  };
  const p5_can_climate_secondary_t climate_secondary = {
      .sequence = sequence,
      .humidity_milli_pct = bme_value_in_range
                                ? measurement->bme280.humidity_milli_pct
                                : P5_CAN_VALUE_U32_INVALID,
      .age_100ms = measurement->bme280.metadata.value_present
                       ? p5_can_age_u16_from_ms(
                             measurement->bme280.metadata.age_ms)
                       : P5_CAN_AGE_U16_UNKNOWN,
  };
  const p5_can_scalar_summary_t illuminance = {
      .sequence = sequence,
      .value = light_value_present
                   ? measurement->veml7700.illuminance_millilux
                   : P5_CAN_VALUE_U32_INVALID,
      .data_flags =
          app_can_data_flags(&measurement->veml7700.metadata, false),
      .age_100ms = light_value_present
                       ? p5_can_age_u8_from_ms(
                             measurement->veml7700.metadata.age_ms)
                       : P5_CAN_AGE_U8_UNKNOWN,
  };
  const p5_can_scalar_summary_t vibration = {
      .sequence = sequence,
      .value = vibration_value_present
                   ? measurement->adxl345_feature.resultant_rms_millig
                   : P5_CAN_VALUE_U32_INVALID,
      .data_flags =
          app_can_data_flags(&measurement->adxl345_feature.metadata, false),
      .age_100ms = vibration_value_present
                       ? p5_can_age_u8_from_ms(
                             measurement->adxl345_feature.metadata.age_ms)
                       : P5_CAN_AGE_U8_UNKNOWN,
  };

  const p5_can_result_t results[6] = {
      p5_can_encode_heartbeat(
          &heartbeat, &frames[APP_CAN_PERIODIC_HEARTBEAT]),
      p5_can_encode_health_summary(
          &health_summary, &frames[APP_CAN_PERIODIC_HEALTH]),
      p5_can_encode_climate_primary(
          &climate_primary, &frames[APP_CAN_PERIODIC_CLIMATE_PRIMARY]),
      p5_can_encode_climate_secondary(
          &climate_secondary, &frames[APP_CAN_PERIODIC_CLIMATE_SECONDARY]),
      p5_can_encode_illuminance(
          &illuminance, &frames[APP_CAN_PERIODIC_ILLUMINANCE]),
      p5_can_encode_vibration_summary(
          &vibration, &frames[APP_CAN_PERIODIC_VIBRATION]),
  };
  return app_can_all_encodes_succeeded(results);
}

uint32_t app_can_tx_pending(const app_can_tx_scheduler_t *scheduler) {
  if (scheduler == NULL) {
    return 0U;
  }

  uint32_t pending = scheduler->event_count +
                     (scheduler->diagnostic_pending ? 1U : 0U);
  for (uint32_t group = 0U; group < APP_CAN_TELEMETRY_GROUP_COUNT; ++group) {
    pending += app_can_count_bits(scheduler->telemetry_pending[group]);
  }
  return pending;
}

static void app_can_update_maximum_pending(app_can_tx_scheduler_t *scheduler) {
  const uint32_t pending = app_can_tx_pending(scheduler);
  if (pending > scheduler->counters.maximum_pending) {
    scheduler->counters.maximum_pending = pending;
  }
}

static bool app_can_frame_matches_group(app_can_telemetry_group_t group,
                                        const p5_can_frame_t *frame) {
  if ((frame == NULL) || (frame->dlc != P5_CAN_DLC)) {
    return false;
  }
  switch (group) {
  case APP_CAN_TELEMETRY_HEARTBEAT:
    return frame->standard_id == P5_CAN_ID_HEARTBEAT;
  case APP_CAN_TELEMETRY_HEALTH:
    return frame->standard_id == P5_CAN_ID_HEALTH_SUMMARY;
  case APP_CAN_TELEMETRY_ILLUMINANCE:
    return frame->standard_id == P5_CAN_ID_ILLUMINANCE;
  case APP_CAN_TELEMETRY_VIBRATION:
    return frame->standard_id == P5_CAN_ID_VIBRATION_SUMMARY;
  default:
    return false;
  }
}

void app_can_tx_scheduler_initialize(app_can_tx_scheduler_t *scheduler) {
  if (scheduler != NULL) {
    (void)memset(scheduler, 0, sizeof(*scheduler));
  }
}

void app_can_diagnostic_initialize(app_can_diagnostic_responder_t *responder) {
  if (responder != NULL) {
    (void)memset(responder, 0, sizeof(*responder));
  }
}

app_can_diagnostic_result_t app_can_diagnostic_process(
    app_can_diagnostic_responder_t *responder,
    const p5_can_frame_t *request,
    uint32_t now_ms,
    p5_can_frame_t *response) {
  if ((responder == NULL) || (request == NULL) || (response == NULL)) {
    return APP_CAN_DIAGNOSTIC_MALFORMED;
  }

  p5_can_diagnostic_request_t decoded;
  if (p5_can_decode_diagnostic_request(request, &decoded) != P5_CAN_RESULT_OK) {
    responder->counters.malformed_requests = app_can_saturating_increment(
        responder->counters.malformed_requests);
    return APP_CAN_DIAGNOSTIC_MALFORMED;
  }
  if (responder->has_last_request &&
      (decoded.sequence == responder->last_sequence) &&
      (decoded.nonce == responder->last_nonce)) {
    responder->counters.duplicate_requests = app_can_saturating_increment(
        responder->counters.duplicate_requests);
    return APP_CAN_DIAGNOSTIC_DUPLICATE;
  }
  if (responder->has_last_request &&
      ((uint32_t)(now_ms - responder->last_accepted_ms) <
       APP_CAN_DIAGNOSTIC_MIN_INTERVAL_MS)) {
    responder->counters.rate_limited_requests = app_can_saturating_increment(
        responder->counters.rate_limited_requests);
    return APP_CAN_DIAGNOSTIC_RATE_LIMITED;
  }

  const p5_can_diagnostic_response_t payload = {
      .sequence = decoded.sequence,
      .status = P5_CAN_DIAGNOSTIC_STATUS_OK,
      .opcode = decoded.opcode,
      .nonce = decoded.nonce,
  };
  if (p5_can_encode_diagnostic_response(&payload, response) !=
      P5_CAN_RESULT_OK) {
    responder->counters.malformed_requests = app_can_saturating_increment(
        responder->counters.malformed_requests);
    return APP_CAN_DIAGNOSTIC_MALFORMED;
  }

  responder->has_last_request = true;
  responder->last_sequence = decoded.sequence;
  responder->last_nonce = decoded.nonce;
  responder->last_accepted_ms = now_ms;
  responder->counters.valid_requests = app_can_saturating_increment(
      responder->counters.valid_requests);
  return APP_CAN_DIAGNOSTIC_READY;
}

app_can_diagnostic_counters_t app_can_diagnostic_counters(
    const app_can_diagnostic_responder_t *responder) {
  const app_can_diagnostic_counters_t empty = {0U};
  return (responder == NULL) ? empty : responder->counters;
}

bool app_can_tx_enqueue_event(app_can_tx_scheduler_t *scheduler,
                              uint16_t event_code, uint8_t source,
                              const p5_can_frame_t *frame) {
  if ((scheduler == NULL) || (frame == NULL) ||
      (frame->standard_id != P5_CAN_ID_STATUS_EVENT) ||
      (frame->dlc != P5_CAN_DLC)) {
    return false;
  }

  for (uint32_t offset = 0U; offset < scheduler->event_count; ++offset) {
    const uint32_t index =
        (scheduler->event_head + offset) % APP_CAN_EVENT_FIFO_DEPTH;
    app_can_event_entry_t *entry = &scheduler->events[index];
    if ((entry->event_code == event_code) && (entry->source == source)) {
      entry->frame = *frame;
      scheduler->counters.event_coalesced =
          app_can_saturating_increment(scheduler->counters.event_coalesced);
      return true;
    }
  }

  if (scheduler->event_count >= APP_CAN_EVENT_FIFO_DEPTH) {
    scheduler->counters.event_dropped =
        app_can_saturating_increment(scheduler->counters.event_dropped);
    return false;
  }

  const uint32_t tail = (scheduler->event_head + scheduler->event_count) %
                        APP_CAN_EVENT_FIFO_DEPTH;
  scheduler->events[tail].event_code = event_code;
  scheduler->events[tail].source = source;
  scheduler->events[tail].frame = *frame;
  ++scheduler->event_count;
  scheduler->counters.event_enqueued =
      app_can_saturating_increment(scheduler->counters.event_enqueued);
  app_can_update_maximum_pending(scheduler);
  return true;
}

bool app_can_tx_publish_diagnostic_response(
    app_can_tx_scheduler_t *scheduler,
    const p5_can_frame_t *frame) {
  p5_can_diagnostic_response_t decoded;
  if ((scheduler == NULL) || (frame == NULL) ||
      (p5_can_decode_diagnostic_response(frame, &decoded) !=
       P5_CAN_RESULT_OK)) {
    return false;
  }
  if (scheduler->diagnostic_pending) {
    scheduler->counters.diagnostic_dropped = app_can_saturating_increment(
        scheduler->counters.diagnostic_dropped);
    return false;
  }
  scheduler->diagnostic_response = *frame;
  scheduler->diagnostic_pending = true;
  scheduler->counters.diagnostic_enqueued = app_can_saturating_increment(
      scheduler->counters.diagnostic_enqueued);
  app_can_update_maximum_pending(scheduler);
  return true;
}

bool app_can_tx_publish_telemetry(app_can_tx_scheduler_t *scheduler,
                                  app_can_telemetry_group_t group,
                                  const p5_can_frame_t *frame) {
  if ((scheduler == NULL) || (group >= APP_CAN_TELEMETRY_GROUP_COUNT) ||
      (group == APP_CAN_TELEMETRY_CLIMATE_PAIR) ||
      !app_can_frame_matches_group(group, frame)) {
    return false;
  }

  if (scheduler->telemetry_pending[group] != 0U) {
    scheduler->counters.telemetry_replaced =
        app_can_saturating_increment(scheduler->counters.telemetry_replaced);
  }
  scheduler->telemetry[group][0] = *frame;
  scheduler->telemetry_pending[group] = UINT8_C(0x01);
  scheduler->counters.telemetry_published =
      app_can_saturating_increment(scheduler->counters.telemetry_published);
  app_can_update_maximum_pending(scheduler);
  return true;
}

bool app_can_tx_publish_climate_pair(app_can_tx_scheduler_t *scheduler,
                                     const p5_can_frame_t *primary,
                                     const p5_can_frame_t *secondary) {
  if ((scheduler == NULL) || (primary == NULL) || (secondary == NULL) ||
      !p5_can_climate_pair_matches(primary, secondary)) {
    return false;
  }

  const uint8_t pending =
      scheduler->telemetry_pending[APP_CAN_TELEMETRY_CLIMATE_PAIR];
  if (pending == UINT8_C(0x02)) {
    scheduler->counters.telemetry_dropped =
        app_can_saturating_add(scheduler->counters.telemetry_dropped, 2U);
    return false;
  }
  if (pending != 0U) {
    scheduler->counters.telemetry_replaced =
        app_can_saturating_add(scheduler->counters.telemetry_replaced, 2U);
  }

  scheduler->telemetry[APP_CAN_TELEMETRY_CLIMATE_PAIR][0] = *primary;
  scheduler->telemetry[APP_CAN_TELEMETRY_CLIMATE_PAIR][1] = *secondary;
  scheduler->telemetry_pending[APP_CAN_TELEMETRY_CLIMATE_PAIR] = UINT8_C(0x03);
  scheduler->counters.telemetry_published =
      app_can_saturating_add(scheduler->counters.telemetry_published, 2U);
  app_can_update_maximum_pending(scheduler);
  return true;
}

static bool app_can_tx_peek_periodic(app_can_tx_scheduler_t *scheduler,
                                     p5_can_frame_t *frame,
                                     app_can_tx_token_t *token) {
  const uint8_t climate_pending =
      scheduler->telemetry_pending[APP_CAN_TELEMETRY_CLIMATE_PAIR];
  if (climate_pending == UINT8_C(0x02)) {
    *frame = scheduler->telemetry[APP_CAN_TELEMETRY_CLIMATE_PAIR][1];
    token->kind = APP_CAN_TX_TOKEN_TELEMETRY;
    token->group = APP_CAN_TELEMETRY_CLIMATE_PAIR;
    token->part = 1U;
    return true;
  }

  for (uint32_t offset = 0U; offset < APP_CAN_TELEMETRY_GROUP_COUNT; ++offset) {
    const uint8_t group = (uint8_t)((scheduler->round_robin_group + offset) %
                                    APP_CAN_TELEMETRY_GROUP_COUNT);
    const uint8_t pending = scheduler->telemetry_pending[group];
    if (pending == 0U) {
      continue;
    }
    const uint8_t part = ((pending & UINT8_C(0x01)) != 0U) ? 0U : 1U;
    *frame = scheduler->telemetry[group][part];
    token->kind = APP_CAN_TX_TOKEN_TELEMETRY;
    token->group = group;
    token->part = part;
    return true;
  }
  return false;
}

bool app_can_tx_peek(app_can_tx_scheduler_t *scheduler, p5_can_frame_t *frame,
                     app_can_tx_token_t *token) {
  if ((scheduler == NULL) || (frame == NULL) || (token == NULL)) {
    return false;
  }
  token->kind = APP_CAN_TX_TOKEN_NONE;
  token->group = 0U;
  token->part = 0U;

  if (scheduler->diagnostic_pending) {
    *frame = scheduler->diagnostic_response;
    token->kind = APP_CAN_TX_TOKEN_DIAGNOSTIC;
  } else {
    const uint32_t pending = app_can_tx_pending(scheduler);
    const bool periodic_pending = pending > scheduler->event_count;
    if ((scheduler->event_count > 0U) &&
        ((scheduler->event_streak < APP_CAN_EVENT_BURST_LIMIT) ||
         !periodic_pending)) {
      *frame = scheduler->events[scheduler->event_head].frame;
      token->kind = APP_CAN_TX_TOKEN_EVENT;
    } else if (!app_can_tx_peek_periodic(scheduler, frame, token)) {
      return false;
    }
  }

  scheduler->counters.tx_peeked =
      app_can_saturating_increment(scheduler->counters.tx_peeked);
  return true;
}

bool app_can_tx_commit(app_can_tx_scheduler_t *scheduler,
                       app_can_tx_token_t token) {
  if (scheduler == NULL) {
    return false;
  }

  if (token.kind == APP_CAN_TX_TOKEN_DIAGNOSTIC) {
    if (!scheduler->diagnostic_pending) {
      return false;
    }
    scheduler->diagnostic_pending = false;
    scheduler->counters.diagnostic_sent = app_can_saturating_increment(
        scheduler->counters.diagnostic_sent);
  } else if (token.kind == APP_CAN_TX_TOKEN_EVENT) {
    if (scheduler->event_count == 0U) {
      return false;
    }
    scheduler->event_head =
        (uint8_t)((scheduler->event_head + 1U) % APP_CAN_EVENT_FIFO_DEPTH);
    --scheduler->event_count;
    if (scheduler->event_streak < APP_CAN_EVENT_BURST_LIMIT) {
      ++scheduler->event_streak;
    }
  } else if (token.kind == APP_CAN_TX_TOKEN_TELEMETRY) {
    if ((token.group >= APP_CAN_TELEMETRY_GROUP_COUNT) || (token.part >= 2U)) {
      return false;
    }
    const uint8_t bit = (uint8_t)(1U << token.part);
    if ((scheduler->telemetry_pending[token.group] & bit) == 0U) {
      return false;
    }
    scheduler->telemetry_pending[token.group] &= (uint8_t)~bit;
    scheduler->round_robin_group =
        (uint8_t)((token.group + 1U) % APP_CAN_TELEMETRY_GROUP_COUNT);
    scheduler->event_streak = 0U;
  } else {
    return false;
  }

  scheduler->counters.tx_committed =
      app_can_saturating_increment(scheduler->counters.tx_committed);
  return true;
}

void app_can_tx_note_hal_busy(app_can_tx_scheduler_t *scheduler) {
  if (scheduler != NULL) {
    scheduler->counters.hal_busy =
        app_can_saturating_increment(scheduler->counters.hal_busy);
  }
}

app_can_tx_counters_t
app_can_tx_counters(const app_can_tx_scheduler_t *scheduler) {
  const app_can_tx_counters_t empty = {0U};
  return (scheduler == NULL) ? empty : scheduler->counters;
}

void app_can_controller_initialize(app_can_controller_t *controller) {
  if (controller != NULL) {
    (void)memset(controller, 0, sizeof(*controller));
    controller->state = APP_CAN_CONTROLLER_STOPPED;
  }
}

bool app_can_controller_begin_initial_start(app_can_controller_t *controller) {
  if ((controller == NULL) ||
      (controller->state != APP_CAN_CONTROLLER_STOPPED)) {
    return false;
  }
  controller->state = APP_CAN_CONTROLLER_STARTING;
  controller->counters.start_attempts =
      app_can_saturating_increment(controller->counters.start_attempts);
  return true;
}

bool app_can_controller_recovery_due(const app_can_controller_t *controller,
                                     uint32_t now_ms) {
  if ((controller == NULL) ||
      ((controller->state != APP_CAN_CONTROLLER_BUS_OFF) &&
       (controller->state != APP_CAN_CONTROLLER_RECOVERY_WAIT))) {
    return false;
  }
  return app_can_time_reached(now_ms, controller->recovery_deadline_ms);
}

bool app_can_controller_begin_recovery(app_can_controller_t *controller,
                                       uint32_t now_ms) {
  if ((controller == NULL) ||
      !app_can_controller_recovery_due(controller, now_ms) ||
      (controller->recovery_attempts_in_episode >=
       APP_CAN_RECOVERY_ATTEMPT_LIMIT)) {
    return false;
  }
  controller->state = APP_CAN_CONTROLLER_STARTING;
  ++controller->recovery_attempts_in_episode;
  controller->counters.recovery_attempts =
      app_can_saturating_increment(controller->counters.recovery_attempts);
  controller->counters.start_attempts =
      app_can_saturating_increment(controller->counters.start_attempts);
  return true;
}

bool app_can_controller_complete_start(app_can_controller_t *controller,
                                       bool success, uint32_t now_ms) {
  if ((controller == NULL) ||
      (controller->state != APP_CAN_CONTROLLER_STARTING)) {
    return false;
  }

  if (success) {
    controller->state = APP_CAN_CONTROLLER_ACTIVE;
    controller->recovery_attempts_in_episode = 0U;
    controller->counters.start_successes =
        app_can_saturating_increment(controller->counters.start_successes);
  } else {
    controller->counters.start_failures =
        app_can_saturating_increment(controller->counters.start_failures);
    if (controller->recovery_attempts_in_episode >=
        APP_CAN_RECOVERY_ATTEMPT_LIMIT) {
      controller->state = APP_CAN_CONTROLLER_RECOVERY_LATCHED;
    } else {
      controller->state = APP_CAN_CONTROLLER_RECOVERY_WAIT;
      controller->recovery_deadline_ms = now_ms + APP_CAN_RECOVERY_DELAY_MS;
    }
  }
  return true;
}

bool app_can_controller_on_error(app_can_controller_t *controller,
                                 uint32_t error_bits, uint32_t now_ms) {
  if (controller == NULL) {
    return false;
  }
  const app_can_controller_state_t previous = controller->state;
  controller->last_error_bits = error_bits;

  if ((error_bits & APP_CAN_ERROR_ARBITRATION_LOST) != 0U) {
    controller->counters.arbitration_lost =
        app_can_saturating_increment(controller->counters.arbitration_lost);
  }
  if ((error_bits & APP_CAN_ERROR_ACK) != 0U) {
    controller->counters.ack_errors =
        app_can_saturating_increment(controller->counters.ack_errors);
  }
  if ((error_bits & APP_CAN_ERROR_TX) != 0U) {
    controller->counters.tx_errors =
        app_can_saturating_increment(controller->counters.tx_errors);
  }
  if ((error_bits & APP_CAN_ERROR_RX_OVERRUN) != 0U) {
    controller->counters.rx_overruns =
        app_can_saturating_increment(controller->counters.rx_overruns);
  }

  if ((error_bits & APP_CAN_ERROR_BUS_OFF) != 0U) {
    if (controller->state != APP_CAN_CONTROLLER_BUS_OFF) {
      controller->counters.bus_off_transitions = app_can_saturating_increment(
          controller->counters.bus_off_transitions);
      controller->recovery_deadline_ms = now_ms + APP_CAN_RECOVERY_DELAY_MS;
    }
    controller->state = APP_CAN_CONTROLLER_BUS_OFF;
  } else if ((controller->state == APP_CAN_CONTROLLER_BUS_OFF) ||
             (controller->state == APP_CAN_CONTROLLER_RECOVERY_WAIT) ||
             (controller->state == APP_CAN_CONTROLLER_RECOVERY_LATCHED)) {
    return false;
  } else if ((error_bits & APP_CAN_ERROR_PASSIVE) != 0U) {
    if (controller->state != APP_CAN_CONTROLLER_PASSIVE) {
      controller->counters.passive_transitions = app_can_saturating_increment(
          controller->counters.passive_transitions);
    }
    controller->state = APP_CAN_CONTROLLER_PASSIVE;
  } else if ((error_bits & APP_CAN_ERROR_WARNING) != 0U) {
    if (controller->state != APP_CAN_CONTROLLER_WARNING) {
      controller->counters.warning_transitions = app_can_saturating_increment(
          controller->counters.warning_transitions);
    }
    controller->state = APP_CAN_CONTROLLER_WARNING;
  }

  return controller->state != previous;
}
