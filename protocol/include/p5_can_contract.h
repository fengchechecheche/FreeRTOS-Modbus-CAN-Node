#ifndef P5_CAN_CONTRACT_H
#define P5_CAN_CONTRACT_H

#include <stdbool.h>
#include <stdint.h>

#define P5_CAN_SCHEMA_REVISION UINT8_C(1)
#define P5_CAN_DLC UINT8_C(8)

#define P5_CAN_ID_STATUS_EVENT UINT16_C(0x140)
#define P5_CAN_ID_HEARTBEAT UINT16_C(0x240)
#define P5_CAN_ID_HEALTH_SUMMARY UINT16_C(0x241)
#define P5_CAN_ID_CLIMATE_PRIMARY UINT16_C(0x340)
#define P5_CAN_ID_CLIMATE_SECONDARY UINT16_C(0x341)
#define P5_CAN_ID_ILLUMINANCE UINT16_C(0x342)
#define P5_CAN_ID_VIBRATION_SUMMARY UINT16_C(0x440)

#define P5_CAN_DATA_STATE_MASK UINT8_C(0x03)
#define P5_CAN_DATA_RETAINED UINT8_C(0x04)
#define P5_CAN_DATA_QUALITY_WARNING UINT8_C(0x08)
#define P5_CAN_DATA_RESERVED_MASK UINT8_C(0xF0)

#define P5_CAN_TEMPERATURE_INVALID INT16_MIN
#define P5_CAN_PRESSURE_INVALID UINT32_C(0x00FFFFFF)
#define P5_CAN_VALUE_U32_INVALID UINT32_MAX
#define P5_CAN_AGE_U8_UNKNOWN UINT8_MAX
#define P5_CAN_AGE_U8_MAX_VALID UINT8_C(0xFE)
#define P5_CAN_AGE_U16_UNKNOWN UINT16_MAX
#define P5_CAN_AGE_U16_MAX_VALID UINT16_C(0xFFFE)

typedef enum
{
  P5_CAN_RESULT_OK = 0,
  P5_CAN_RESULT_NULL_ARGUMENT,
  P5_CAN_RESULT_INVALID_IDENTIFIER,
  P5_CAN_RESULT_INVALID_DLC,
  P5_CAN_RESULT_INVALID_SCHEMA,
  P5_CAN_RESULT_OUT_OF_RANGE,
  P5_CAN_RESULT_RESERVED_BITS
} p5_can_result_t;

typedef struct
{
  uint16_t standard_id;
  uint8_t dlc;
  uint8_t data[P5_CAN_DLC];
} p5_can_frame_t;

typedef struct
{
  uint8_t sequence;
  uint16_t event_code;
  uint8_t severity;
  uint8_t source;
  uint16_t detail;
} p5_can_status_event_t;

typedef struct
{
  uint8_t sequence;
  uint32_t uptime_s;
  uint16_t image_generation_low16;
} p5_can_heartbeat_t;

typedef struct
{
  uint8_t sequence;
  uint8_t health_state;
  uint8_t source_state_pack;
  uint32_t warning_mask;
} p5_can_health_summary_t;

typedef struct
{
  uint8_t sequence;
  int16_t temperature_centi_c;
  uint32_t pressure_pa;
  uint8_t data_flags;
} p5_can_climate_primary_t;

typedef struct
{
  uint8_t sequence;
  uint32_t humidity_milli_pct;
  uint16_t age_100ms;
} p5_can_climate_secondary_t;

typedef struct
{
  uint8_t sequence;
  uint32_t value;
  uint8_t data_flags;
  uint8_t age_100ms;
} p5_can_scalar_summary_t;

p5_can_result_t p5_can_encode_status_event(
    const p5_can_status_event_t *payload,
    p5_can_frame_t *frame);
p5_can_result_t p5_can_decode_status_event(
    const p5_can_frame_t *frame,
    p5_can_status_event_t *payload);

p5_can_result_t p5_can_encode_heartbeat(
    const p5_can_heartbeat_t *payload,
    p5_can_frame_t *frame);
p5_can_result_t p5_can_decode_heartbeat(
    const p5_can_frame_t *frame,
    p5_can_heartbeat_t *payload);

p5_can_result_t p5_can_encode_health_summary(
    const p5_can_health_summary_t *payload,
    p5_can_frame_t *frame);
p5_can_result_t p5_can_decode_health_summary(
    const p5_can_frame_t *frame,
    p5_can_health_summary_t *payload);

p5_can_result_t p5_can_encode_climate_primary(
    const p5_can_climate_primary_t *payload,
    p5_can_frame_t *frame);
p5_can_result_t p5_can_decode_climate_primary(
    const p5_can_frame_t *frame,
    p5_can_climate_primary_t *payload);

p5_can_result_t p5_can_encode_climate_secondary(
    const p5_can_climate_secondary_t *payload,
    p5_can_frame_t *frame);
p5_can_result_t p5_can_decode_climate_secondary(
    const p5_can_frame_t *frame,
    p5_can_climate_secondary_t *payload);

p5_can_result_t p5_can_encode_illuminance(
    const p5_can_scalar_summary_t *payload,
    p5_can_frame_t *frame);
p5_can_result_t p5_can_decode_illuminance(
    const p5_can_frame_t *frame,
    p5_can_scalar_summary_t *payload);

p5_can_result_t p5_can_encode_vibration_summary(
    const p5_can_scalar_summary_t *payload,
    p5_can_frame_t *frame);
p5_can_result_t p5_can_decode_vibration_summary(
    const p5_can_frame_t *frame,
    p5_can_scalar_summary_t *payload);

p5_can_result_t p5_can_pack_source_states(
    const uint8_t source_states[4],
    uint8_t *packed);
p5_can_result_t p5_can_unpack_source_states(
    uint8_t packed,
    uint8_t source_states[4]);

uint8_t p5_can_age_u8_from_ms(uint32_t age_ms);
uint16_t p5_can_age_u16_from_ms(uint32_t age_ms);
bool p5_can_sequence_is_next(uint8_t previous, uint8_t current);
bool p5_can_climate_pair_matches(
    const p5_can_frame_t *primary,
    const p5_can_frame_t *secondary);

#endif
