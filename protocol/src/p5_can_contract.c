#include "p5_can_contract.h"

#include <stddef.h>

#define P5_CAN_MAX_SEVERITY UINT8_C(3)
#define P5_CAN_MAX_EVENT_SOURCE UINT8_C(5)
#define P5_CAN_MAX_HEALTH_STATE UINT8_C(5)
#define P5_CAN_MAX_MEASUREMENT_STATE UINT8_C(3)
#define P5_CAN_HUMIDITY_MAX_MILLI_PCT UINT32_C(100000)

static void p5_can_put_u16_le(uint8_t *destination, uint16_t value)
{
  destination[0] = (uint8_t)(value & UINT16_C(0x00FF));
  destination[1] = (uint8_t)(value >> 8U);
}

static void p5_can_put_u24_le(uint8_t *destination, uint32_t value)
{
  destination[0] = (uint8_t)(value & UINT32_C(0x000000FF));
  destination[1] = (uint8_t)((value >> 8U) & UINT32_C(0x000000FF));
  destination[2] = (uint8_t)((value >> 16U) & UINT32_C(0x000000FF));
}

static void p5_can_put_u32_le(uint8_t *destination, uint32_t value)
{
  destination[0] = (uint8_t)(value & UINT32_C(0x000000FF));
  destination[1] = (uint8_t)((value >> 8U) & UINT32_C(0x000000FF));
  destination[2] = (uint8_t)((value >> 16U) & UINT32_C(0x000000FF));
  destination[3] = (uint8_t)(value >> 24U);
}

static uint16_t p5_can_get_u16_le(const uint8_t *source)
{
  return (uint16_t)((uint16_t)source[0] |
                    ((uint16_t)source[1] << 8U));
}

static uint32_t p5_can_get_u24_le(const uint8_t *source)
{
  return (uint32_t)source[0] |
         ((uint32_t)source[1] << 8U) |
         ((uint32_t)source[2] << 16U);
}

static uint32_t p5_can_get_u32_le(const uint8_t *source)
{
  return (uint32_t)source[0] |
         ((uint32_t)source[1] << 8U) |
         ((uint32_t)source[2] << 16U) |
         ((uint32_t)source[3] << 24U);
}

static p5_can_result_t p5_can_initialize_frame(
    uint16_t identifier,
    uint8_t sequence,
    p5_can_frame_t *frame)
{
  if (frame == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }

  frame->standard_id = identifier;
  frame->dlc = P5_CAN_DLC;
  for (size_t index = 0U; index < P5_CAN_DLC; ++index)
  {
    frame->data[index] = 0U;
  }
  frame->data[0] = P5_CAN_SCHEMA_REVISION;
  frame->data[1] = sequence;
  return P5_CAN_RESULT_OK;
}

static p5_can_result_t p5_can_validate_frame(
    const p5_can_frame_t *frame,
    uint16_t expected_identifier)
{
  if (frame == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  if (frame->standard_id != expected_identifier)
  {
    return P5_CAN_RESULT_INVALID_IDENTIFIER;
  }
  if (frame->dlc != P5_CAN_DLC)
  {
    return P5_CAN_RESULT_INVALID_DLC;
  }
  if (frame->data[0] != P5_CAN_SCHEMA_REVISION)
  {
    return P5_CAN_RESULT_INVALID_SCHEMA;
  }
  return P5_CAN_RESULT_OK;
}

static p5_can_result_t p5_can_validate_data_flags(uint8_t flags)
{
  if ((flags & P5_CAN_DATA_RESERVED_MASK) != 0U)
  {
    return P5_CAN_RESULT_RESERVED_BITS;
  }
  if ((flags & P5_CAN_DATA_STATE_MASK) > P5_CAN_MAX_MEASUREMENT_STATE)
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_encode_diagnostic_request(
    const p5_can_diagnostic_request_t *payload,
    p5_can_frame_t *frame)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  if (payload->opcode != P5_CAN_DIAGNOSTIC_OPCODE_PING)
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }

  p5_can_result_t result = p5_can_initialize_frame(
      P5_CAN_ID_DIAGNOSTIC_REQUEST, payload->sequence, frame);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  frame->data[2] = payload->opcode;
  frame->data[3] = 0U;
  p5_can_put_u32_le(&frame->data[4], payload->nonce);
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_decode_diagnostic_request(
    const p5_can_frame_t *frame,
    p5_can_diagnostic_request_t *payload)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  p5_can_result_t result = p5_can_validate_frame(
      frame, P5_CAN_ID_DIAGNOSTIC_REQUEST);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  if (frame->data[2] != P5_CAN_DIAGNOSTIC_OPCODE_PING)
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }
  if (frame->data[3] != 0U)
  {
    return P5_CAN_RESULT_RESERVED_BITS;
  }
  payload->sequence = frame->data[1];
  payload->opcode = frame->data[2];
  payload->nonce = p5_can_get_u32_le(&frame->data[4]);
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_encode_diagnostic_response(
    const p5_can_diagnostic_response_t *payload,
    p5_can_frame_t *frame)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  if ((payload->status != P5_CAN_DIAGNOSTIC_STATUS_OK) ||
      (payload->opcode != P5_CAN_DIAGNOSTIC_OPCODE_PING))
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }

  p5_can_result_t result = p5_can_initialize_frame(
      P5_CAN_ID_DIAGNOSTIC_RESPONSE, payload->sequence, frame);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  frame->data[2] = payload->status;
  frame->data[3] = payload->opcode;
  p5_can_put_u32_le(&frame->data[4], payload->nonce);
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_decode_diagnostic_response(
    const p5_can_frame_t *frame,
    p5_can_diagnostic_response_t *payload)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  p5_can_result_t result = p5_can_validate_frame(
      frame, P5_CAN_ID_DIAGNOSTIC_RESPONSE);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  if ((frame->data[2] != P5_CAN_DIAGNOSTIC_STATUS_OK) ||
      (frame->data[3] != P5_CAN_DIAGNOSTIC_OPCODE_PING))
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }
  payload->sequence = frame->data[1];
  payload->status = frame->data[2];
  payload->opcode = frame->data[3];
  payload->nonce = p5_can_get_u32_le(&frame->data[4]);
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_encode_status_event(
    const p5_can_status_event_t *payload,
    p5_can_frame_t *frame)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  if ((payload->severity > P5_CAN_MAX_SEVERITY) ||
      (payload->source > P5_CAN_MAX_EVENT_SOURCE))
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }

  p5_can_result_t result = p5_can_initialize_frame(
      P5_CAN_ID_STATUS_EVENT, payload->sequence, frame);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  p5_can_put_u16_le(&frame->data[2], payload->event_code);
  frame->data[4] = payload->severity;
  frame->data[5] = payload->source;
  p5_can_put_u16_le(&frame->data[6], payload->detail);
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_decode_status_event(
    const p5_can_frame_t *frame,
    p5_can_status_event_t *payload)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  p5_can_result_t result = p5_can_validate_frame(
      frame, P5_CAN_ID_STATUS_EVENT);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  if ((frame->data[4] > P5_CAN_MAX_SEVERITY) ||
      (frame->data[5] > P5_CAN_MAX_EVENT_SOURCE))
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }
  payload->sequence = frame->data[1];
  payload->event_code = p5_can_get_u16_le(&frame->data[2]);
  payload->severity = frame->data[4];
  payload->source = frame->data[5];
  payload->detail = p5_can_get_u16_le(&frame->data[6]);
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_encode_heartbeat(
    const p5_can_heartbeat_t *payload,
    p5_can_frame_t *frame)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  p5_can_result_t result = p5_can_initialize_frame(
      P5_CAN_ID_HEARTBEAT, payload->sequence, frame);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  p5_can_put_u32_le(&frame->data[2], payload->uptime_s);
  p5_can_put_u16_le(&frame->data[6], payload->image_generation_low16);
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_decode_heartbeat(
    const p5_can_frame_t *frame,
    p5_can_heartbeat_t *payload)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  p5_can_result_t result = p5_can_validate_frame(frame, P5_CAN_ID_HEARTBEAT);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  payload->sequence = frame->data[1];
  payload->uptime_s = p5_can_get_u32_le(&frame->data[2]);
  payload->image_generation_low16 = p5_can_get_u16_le(&frame->data[6]);
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_encode_health_summary(
    const p5_can_health_summary_t *payload,
    p5_can_frame_t *frame)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  if (payload->health_state > P5_CAN_MAX_HEALTH_STATE)
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }
  p5_can_result_t result = p5_can_initialize_frame(
      P5_CAN_ID_HEALTH_SUMMARY, payload->sequence, frame);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  frame->data[2] = payload->health_state;
  frame->data[3] = payload->source_state_pack;
  p5_can_put_u32_le(&frame->data[4], payload->warning_mask);
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_decode_health_summary(
    const p5_can_frame_t *frame,
    p5_can_health_summary_t *payload)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  p5_can_result_t result = p5_can_validate_frame(
      frame, P5_CAN_ID_HEALTH_SUMMARY);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  if (frame->data[2] > P5_CAN_MAX_HEALTH_STATE)
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }
  payload->sequence = frame->data[1];
  payload->health_state = frame->data[2];
  payload->source_state_pack = frame->data[3];
  payload->warning_mask = p5_can_get_u32_le(&frame->data[4]);
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_encode_climate_primary(
    const p5_can_climate_primary_t *payload,
    p5_can_frame_t *frame)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  if (payload->pressure_pa > P5_CAN_PRESSURE_INVALID)
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }
  p5_can_result_t result = p5_can_validate_data_flags(payload->data_flags);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  result = p5_can_initialize_frame(
      P5_CAN_ID_CLIMATE_PRIMARY, payload->sequence, frame);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  p5_can_put_u16_le(
      &frame->data[2], (uint16_t)payload->temperature_centi_c);
  p5_can_put_u24_le(&frame->data[4], payload->pressure_pa);
  frame->data[7] = payload->data_flags;
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_decode_climate_primary(
    const p5_can_frame_t *frame,
    p5_can_climate_primary_t *payload)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  p5_can_result_t result = p5_can_validate_frame(
      frame, P5_CAN_ID_CLIMATE_PRIMARY);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  result = p5_can_validate_data_flags(frame->data[7]);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  payload->sequence = frame->data[1];
  payload->temperature_centi_c = (int16_t)p5_can_get_u16_le(&frame->data[2]);
  payload->pressure_pa = p5_can_get_u24_le(&frame->data[4]);
  payload->data_flags = frame->data[7];
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_encode_climate_secondary(
    const p5_can_climate_secondary_t *payload,
    p5_can_frame_t *frame)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  if ((payload->humidity_milli_pct > P5_CAN_HUMIDITY_MAX_MILLI_PCT) &&
      (payload->humidity_milli_pct != P5_CAN_VALUE_U32_INVALID))
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }
  p5_can_result_t result = p5_can_initialize_frame(
      P5_CAN_ID_CLIMATE_SECONDARY, payload->sequence, frame);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  p5_can_put_u32_le(&frame->data[2], payload->humidity_milli_pct);
  p5_can_put_u16_le(&frame->data[6], payload->age_100ms);
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_decode_climate_secondary(
    const p5_can_frame_t *frame,
    p5_can_climate_secondary_t *payload)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  p5_can_result_t result = p5_can_validate_frame(
      frame, P5_CAN_ID_CLIMATE_SECONDARY);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  const uint32_t humidity = p5_can_get_u32_le(&frame->data[2]);
  if ((humidity > P5_CAN_HUMIDITY_MAX_MILLI_PCT) &&
      (humidity != P5_CAN_VALUE_U32_INVALID))
  {
    return P5_CAN_RESULT_OUT_OF_RANGE;
  }
  payload->sequence = frame->data[1];
  payload->humidity_milli_pct = humidity;
  payload->age_100ms = p5_can_get_u16_le(&frame->data[6]);
  return P5_CAN_RESULT_OK;
}

static p5_can_result_t p5_can_encode_scalar(
    uint16_t identifier,
    const p5_can_scalar_summary_t *payload,
    p5_can_frame_t *frame)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  p5_can_result_t result = p5_can_validate_data_flags(payload->data_flags);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  result = p5_can_initialize_frame(identifier, payload->sequence, frame);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  p5_can_put_u32_le(&frame->data[2], payload->value);
  frame->data[6] = payload->data_flags;
  frame->data[7] = payload->age_100ms;
  return P5_CAN_RESULT_OK;
}

static p5_can_result_t p5_can_decode_scalar(
    uint16_t identifier,
    const p5_can_frame_t *frame,
    p5_can_scalar_summary_t *payload)
{
  if (payload == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  p5_can_result_t result = p5_can_validate_frame(frame, identifier);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  result = p5_can_validate_data_flags(frame->data[6]);
  if (result != P5_CAN_RESULT_OK)
  {
    return result;
  }
  payload->sequence = frame->data[1];
  payload->value = p5_can_get_u32_le(&frame->data[2]);
  payload->data_flags = frame->data[6];
  payload->age_100ms = frame->data[7];
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_encode_illuminance(
    const p5_can_scalar_summary_t *payload,
    p5_can_frame_t *frame)
{
  return p5_can_encode_scalar(P5_CAN_ID_ILLUMINANCE, payload, frame);
}

p5_can_result_t p5_can_decode_illuminance(
    const p5_can_frame_t *frame,
    p5_can_scalar_summary_t *payload)
{
  return p5_can_decode_scalar(P5_CAN_ID_ILLUMINANCE, frame, payload);
}

p5_can_result_t p5_can_encode_vibration_summary(
    const p5_can_scalar_summary_t *payload,
    p5_can_frame_t *frame)
{
  return p5_can_encode_scalar(P5_CAN_ID_VIBRATION_SUMMARY, payload, frame);
}

p5_can_result_t p5_can_decode_vibration_summary(
    const p5_can_frame_t *frame,
    p5_can_scalar_summary_t *payload)
{
  return p5_can_decode_scalar(P5_CAN_ID_VIBRATION_SUMMARY, frame, payload);
}

p5_can_result_t p5_can_pack_source_states(
    const uint8_t source_states[4],
    uint8_t *packed)
{
  if ((source_states == NULL) || (packed == NULL))
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  uint8_t result = 0U;
  for (uint8_t index = 0U; index < 4U; ++index)
  {
    if (source_states[index] > P5_CAN_MAX_MEASUREMENT_STATE)
    {
      return P5_CAN_RESULT_OUT_OF_RANGE;
    }
    result |= (uint8_t)(source_states[index] << (index * 2U));
  }
  *packed = result;
  return P5_CAN_RESULT_OK;
}

p5_can_result_t p5_can_unpack_source_states(
    uint8_t packed,
    uint8_t source_states[4])
{
  if (source_states == NULL)
  {
    return P5_CAN_RESULT_NULL_ARGUMENT;
  }
  for (uint8_t index = 0U; index < 4U; ++index)
  {
    source_states[index] = (uint8_t)((packed >> (index * 2U)) & UINT8_C(0x03));
  }
  return P5_CAN_RESULT_OK;
}

uint8_t p5_can_age_u8_from_ms(uint32_t age_ms)
{
  const uint32_t units = age_ms / UINT32_C(100);
  return units > P5_CAN_AGE_U8_MAX_VALID
             ? P5_CAN_AGE_U8_MAX_VALID
             : (uint8_t)units;
}

uint16_t p5_can_age_u16_from_ms(uint32_t age_ms)
{
  const uint32_t units = age_ms / UINT32_C(100);
  return units > P5_CAN_AGE_U16_MAX_VALID
             ? P5_CAN_AGE_U16_MAX_VALID
             : (uint16_t)units;
}

bool p5_can_sequence_is_next(uint8_t previous, uint8_t current)
{
  return (uint8_t)(previous + UINT8_C(1)) == current;
}

bool p5_can_climate_pair_matches(
    const p5_can_frame_t *primary,
    const p5_can_frame_t *secondary)
{
  return (p5_can_validate_frame(primary, P5_CAN_ID_CLIMATE_PRIMARY) ==
              P5_CAN_RESULT_OK) &&
         (p5_can_validate_frame(secondary, P5_CAN_ID_CLIMATE_SECONDARY) ==
              P5_CAN_RESULT_OK) &&
         (primary->data[1] == secondary->data[1]);
}
