#include "p5_can_contract.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)          \
  do                              \
  {                               \
    if (!(condition))             \
    {                             \
      return EXIT_FAILURE;        \
    }                             \
  } while (false)

static bool frame_equals(
    const p5_can_frame_t *frame,
    uint16_t identifier,
    const uint8_t expected[P5_CAN_DLC])
{
  return (frame->standard_id == identifier) &&
         (frame->dlc == P5_CAN_DLC) &&
         (memcmp(frame->data, expected, P5_CAN_DLC) == 0);
}

static int test_status_and_heartbeat(void)
{
  static const uint8_t event_expected[P5_CAN_DLC] =
      {0x01U, 0x2AU, 0x02U, 0x01U, 0x01U, 0x05U, 0x34U, 0x12U};
  static const uint8_t heartbeat_expected[P5_CAN_DLC] =
      {0x01U, 0xFEU, 0x78U, 0x56U, 0x34U, 0x12U, 0xCDU, 0xABU};
  const p5_can_status_event_t event = {
      0x2AU, UINT16_C(0x0102), 1U, 5U, UINT16_C(0x1234)};
  const p5_can_heartbeat_t heartbeat = {
      0xFEU, UINT32_C(0x12345678), UINT16_C(0xABCD)};
  p5_can_status_event_t decoded_event = {0U, 0U, 0U, 0U, 0U};
  p5_can_heartbeat_t decoded_heartbeat = {0U, 0U, 0U};
  p5_can_frame_t frame = {0U, 0U, {0U}};

  CHECK(p5_can_encode_status_event(&event, &frame) == P5_CAN_RESULT_OK);
  CHECK(frame_equals(&frame, P5_CAN_ID_STATUS_EVENT, event_expected));
  CHECK(p5_can_decode_status_event(&frame, &decoded_event) == P5_CAN_RESULT_OK);
  CHECK(decoded_event.sequence == event.sequence);
  CHECK(decoded_event.event_code == event.event_code);
  CHECK(decoded_event.severity == event.severity);
  CHECK(decoded_event.source == event.source);
  CHECK(decoded_event.detail == event.detail);

  CHECK(p5_can_encode_heartbeat(&heartbeat, &frame) == P5_CAN_RESULT_OK);
  CHECK(frame_equals(&frame, P5_CAN_ID_HEARTBEAT, heartbeat_expected));
  CHECK(p5_can_decode_heartbeat(&frame, &decoded_heartbeat) == P5_CAN_RESULT_OK);
  CHECK(decoded_heartbeat.sequence == heartbeat.sequence);
  CHECK(decoded_heartbeat.uptime_s == heartbeat.uptime_s);
  CHECK(decoded_heartbeat.image_generation_low16 == heartbeat.image_generation_low16);
  return EXIT_SUCCESS;
}

static int test_health_and_source_states(void)
{
  static const uint8_t expected[P5_CAN_DLC] =
      {0x01U, 0x10U, 0x02U, 0xE4U, 0x21U, 0x43U, 0x65U, 0x87U};
  const uint8_t states[4] = {0U, 1U, 2U, 3U};
  uint8_t unpacked[4] = {0U, 0U, 0U, 0U};
  uint8_t packed = 0U;
  p5_can_frame_t frame = {0U, 0U, {0U}};
  p5_can_health_summary_t decoded = {0U, 0U, 0U, 0U};

  CHECK(p5_can_pack_source_states(states, &packed) == P5_CAN_RESULT_OK);
  CHECK(packed == 0xE4U);
  const p5_can_health_summary_t health = {
      0x10U, 2U, packed, UINT32_C(0x87654321)};
  CHECK(p5_can_encode_health_summary(&health, &frame) == P5_CAN_RESULT_OK);
  CHECK(frame_equals(&frame, P5_CAN_ID_HEALTH_SUMMARY, expected));
  CHECK(p5_can_decode_health_summary(&frame, &decoded) == P5_CAN_RESULT_OK);
  CHECK(decoded.warning_mask == health.warning_mask);
  CHECK(p5_can_unpack_source_states(decoded.source_state_pack, unpacked) ==
        P5_CAN_RESULT_OK);
  CHECK(memcmp(states, unpacked, sizeof(states)) == 0);
  return EXIT_SUCCESS;
}

static int test_climate_pair(void)
{
  static const uint8_t primary_expected[P5_CAN_DLC] =
      {0x01U, 0x7FU, 0x2EU, 0xFBU, 0xA0U, 0x86U, 0x01U, 0x0DU};
  static const uint8_t secondary_expected[P5_CAN_DLC] =
      {0x01U, 0x7FU, 0x50U, 0xC3U, 0x00U, 0x00U, 0x19U, 0x00U};
  const p5_can_climate_primary_t primary = {
      0x7FU, -1234, UINT32_C(100000), 0x0DU};
  const p5_can_climate_secondary_t secondary = {
      0x7FU, UINT32_C(50000), UINT16_C(25)};
  p5_can_climate_primary_t decoded_primary = {0U, 0, 0U, 0U};
  p5_can_climate_secondary_t decoded_secondary = {0U, 0U, 0U};
  p5_can_frame_t primary_frame = {0U, 0U, {0U}};
  p5_can_frame_t secondary_frame = {0U, 0U, {0U}};

  CHECK(p5_can_encode_climate_primary(&primary, &primary_frame) ==
        P5_CAN_RESULT_OK);
  CHECK(p5_can_encode_climate_secondary(&secondary, &secondary_frame) ==
        P5_CAN_RESULT_OK);
  CHECK(frame_equals(
      &primary_frame, P5_CAN_ID_CLIMATE_PRIMARY, primary_expected));
  CHECK(frame_equals(
      &secondary_frame, P5_CAN_ID_CLIMATE_SECONDARY, secondary_expected));
  CHECK(p5_can_climate_pair_matches(&primary_frame, &secondary_frame));
  secondary_frame.data[1] = 0x80U;
  CHECK(!p5_can_climate_pair_matches(&primary_frame, &secondary_frame));
  secondary_frame.data[1] = 0x7FU;

  CHECK(p5_can_decode_climate_primary(&primary_frame, &decoded_primary) ==
        P5_CAN_RESULT_OK);
  CHECK(p5_can_decode_climate_secondary(&secondary_frame, &decoded_secondary) ==
        P5_CAN_RESULT_OK);
  CHECK(decoded_primary.temperature_centi_c == primary.temperature_centi_c);
  CHECK(decoded_primary.pressure_pa == primary.pressure_pa);
  CHECK(decoded_primary.data_flags == primary.data_flags);
  CHECK(decoded_secondary.humidity_milli_pct == secondary.humidity_milli_pct);
  CHECK(decoded_secondary.age_100ms == secondary.age_100ms);
  return EXIT_SUCCESS;
}

static int test_scalar_frames_and_boundaries(void)
{
  static const uint8_t illuminance_expected[P5_CAN_DLC] =
      {0x01U, 0xFFU, 0x40U, 0xE2U, 0x01U, 0x00U, 0x01U, 0x14U};
  static const uint8_t vibration_expected[P5_CAN_DLC] =
      {0x01U, 0x00U, 0xE1U, 0x10U, 0x00U, 0x00U, 0x0AU, 0xFEU};
  const p5_can_scalar_summary_t illuminance = {
      0xFFU, UINT32_C(123456), 0x01U, 20U};
  const p5_can_scalar_summary_t vibration = {
      0x00U, UINT32_C(4321), 0x0AU, P5_CAN_AGE_U8_MAX_VALID};
  p5_can_scalar_summary_t decoded = {0U, 0U, 0U, 0U};
  p5_can_frame_t frame = {0U, 0U, {0U}};

  CHECK(p5_can_encode_illuminance(&illuminance, &frame) == P5_CAN_RESULT_OK);
  CHECK(frame_equals(&frame, P5_CAN_ID_ILLUMINANCE, illuminance_expected));
  CHECK(p5_can_decode_illuminance(&frame, &decoded) == P5_CAN_RESULT_OK);
  CHECK(decoded.value == illuminance.value);

  CHECK(p5_can_encode_vibration_summary(&vibration, &frame) ==
        P5_CAN_RESULT_OK);
  CHECK(frame_equals(
      &frame, P5_CAN_ID_VIBRATION_SUMMARY, vibration_expected));
  CHECK(p5_can_decode_vibration_summary(&frame, &decoded) ==
        P5_CAN_RESULT_OK);
  CHECK(decoded.value == vibration.value);
  CHECK(decoded.age_100ms == P5_CAN_AGE_U8_MAX_VALID);

  CHECK(p5_can_age_u8_from_ms(UINT32_C(25399)) == 253U);
  CHECK(p5_can_age_u8_from_ms(UINT32_C(25400)) == P5_CAN_AGE_U8_MAX_VALID);
  CHECK(p5_can_age_u8_from_ms(UINT32_MAX) == P5_CAN_AGE_U8_MAX_VALID);
  CHECK(p5_can_age_u16_from_ms(UINT32_C(1234)) == 12U);
  CHECK(p5_can_sequence_is_next(0xFEU, 0xFFU));
  CHECK(p5_can_sequence_is_next(0xFFU, 0x00U));
  CHECK(!p5_can_sequence_is_next(0xFFU, 0x01U));
  return EXIT_SUCCESS;
}

static int test_diagnostic_ping(void)
{
  static const uint8_t request_expected[P5_CAN_DLC] =
      {0x01U, 0x2AU, 0x01U, 0x00U, 0x78U, 0x56U, 0x34U, 0x12U};
  static const uint8_t response_expected[P5_CAN_DLC] =
      {0x01U, 0x2AU, 0x00U, 0x01U, 0x78U, 0x56U, 0x34U, 0x12U};
  const p5_can_diagnostic_request_t request = {
      0x2AU, P5_CAN_DIAGNOSTIC_OPCODE_PING, UINT32_C(0x12345678)};
  const p5_can_diagnostic_response_t response = {
      0x2AU, P5_CAN_DIAGNOSTIC_STATUS_OK,
      P5_CAN_DIAGNOSTIC_OPCODE_PING, UINT32_C(0x12345678)};
  p5_can_diagnostic_request_t decoded_request = {0U, 0U, 0U};
  p5_can_diagnostic_response_t decoded_response = {0U, 0U, 0U, 0U};
  p5_can_frame_t frame = {0U, 0U, {0U}};

  CHECK(p5_can_encode_diagnostic_request(&request, &frame) ==
        P5_CAN_RESULT_OK);
  CHECK(frame_equals(&frame, P5_CAN_ID_DIAGNOSTIC_REQUEST,
                     request_expected));
  CHECK(p5_can_decode_diagnostic_request(&frame, &decoded_request) ==
        P5_CAN_RESULT_OK);
  CHECK(decoded_request.sequence == request.sequence);
  CHECK(decoded_request.opcode == request.opcode);
  CHECK(decoded_request.nonce == request.nonce);

  CHECK(p5_can_encode_diagnostic_response(&response, &frame) ==
        P5_CAN_RESULT_OK);
  CHECK(frame_equals(&frame, P5_CAN_ID_DIAGNOSTIC_RESPONSE,
                     response_expected));
  CHECK(p5_can_decode_diagnostic_response(&frame, &decoded_response) ==
        P5_CAN_RESULT_OK);
  CHECK(decoded_response.sequence == response.sequence);
  CHECK(decoded_response.status == response.status);
  CHECK(decoded_response.opcode == response.opcode);
  CHECK(decoded_response.nonce == response.nonce);

  frame = (p5_can_frame_t){
      P5_CAN_ID_DIAGNOSTIC_REQUEST, P5_CAN_DLC,
      {0x01U, 0x2AU, 0x01U, 0x01U, 0x78U, 0x56U, 0x34U, 0x12U}};
  CHECK(p5_can_decode_diagnostic_request(&frame, &decoded_request) ==
        P5_CAN_RESULT_RESERVED_BITS);
  frame.data[3] = 0U;
  frame.data[2] = 2U;
  CHECK(p5_can_decode_diagnostic_request(&frame, &decoded_request) ==
        P5_CAN_RESULT_OUT_OF_RANGE);
  return EXIT_SUCCESS;
}

static int test_invalid_inputs(void)
{
  p5_can_frame_t frame = {0U, 0U, {0U}};
  p5_can_status_event_t event = {0U, 1U, 4U, 0U, 0U};
  p5_can_climate_primary_t primary = {0U, 0, UINT32_C(0x01000000), 1U};
  p5_can_climate_secondary_t secondary = {0U, UINT32_C(100001), 0U};
  p5_can_scalar_summary_t scalar = {0U, 0U, 0xF1U, 0U};
  const uint8_t bad_states[4] = {0U, 1U, 2U, 4U};
  uint8_t packed = 0U;

  CHECK(p5_can_encode_status_event(&event, &frame) ==
        P5_CAN_RESULT_OUT_OF_RANGE);
  event.severity = 0U;
  event.source = 6U;
  CHECK(p5_can_encode_status_event(&event, &frame) ==
        P5_CAN_RESULT_OUT_OF_RANGE);
  CHECK(p5_can_encode_climate_primary(&primary, &frame) ==
        P5_CAN_RESULT_OUT_OF_RANGE);
  CHECK(p5_can_encode_climate_secondary(&secondary, &frame) ==
        P5_CAN_RESULT_OUT_OF_RANGE);
  CHECK(p5_can_encode_illuminance(&scalar, &frame) ==
        P5_CAN_RESULT_RESERVED_BITS);
  CHECK(p5_can_pack_source_states(bad_states, &packed) ==
        P5_CAN_RESULT_OUT_OF_RANGE);

  scalar.data_flags = 1U;
  CHECK(p5_can_encode_illuminance(&scalar, &frame) == P5_CAN_RESULT_OK);
  frame.standard_id = P5_CAN_ID_VIBRATION_SUMMARY;
  CHECK(p5_can_decode_illuminance(&frame, &scalar) ==
        P5_CAN_RESULT_INVALID_IDENTIFIER);
  frame.standard_id = P5_CAN_ID_ILLUMINANCE;
  frame.dlc = 7U;
  CHECK(p5_can_decode_illuminance(&frame, &scalar) ==
        P5_CAN_RESULT_INVALID_DLC);
  frame.dlc = P5_CAN_DLC;
  frame.data[0] = 2U;
  CHECK(p5_can_decode_illuminance(&frame, &scalar) ==
        P5_CAN_RESULT_INVALID_SCHEMA);
  frame.data[0] = P5_CAN_SCHEMA_REVISION;
  frame.data[6] = 0xF1U;
  CHECK(p5_can_decode_illuminance(&frame, &scalar) ==
        P5_CAN_RESULT_RESERVED_BITS);

  CHECK(p5_can_encode_heartbeat(NULL, &frame) ==
        P5_CAN_RESULT_NULL_ARGUMENT);
  CHECK(p5_can_decode_heartbeat(&frame, NULL) ==
        P5_CAN_RESULT_NULL_ARGUMENT);
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(test_status_and_heartbeat() == EXIT_SUCCESS);
  CHECK(test_health_and_source_states() == EXIT_SUCCESS);
  CHECK(test_climate_pair() == EXIT_SUCCESS);
  CHECK(test_scalar_frames_and_boundaries() == EXIT_SUCCESS);
  CHECK(test_diagnostic_ping() == EXIT_SUCCESS);
  CHECK(test_invalid_inputs() == EXIT_SUCCESS);
  puts("P5 host CAN contract: PASS");
  return EXIT_SUCCESS;
}
