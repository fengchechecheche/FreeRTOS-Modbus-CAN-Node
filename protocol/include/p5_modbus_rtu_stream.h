#ifndef P5_MODBUS_RTU_STREAM_H
#define P5_MODBUS_RTU_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "p5_modbus_rtu_adu.h"

typedef enum
{
  P5_MODBUS_STREAM_IDLE = 0,
  P5_MODBUS_STREAM_RECEIVING,
  P5_MODBUS_STREAM_DISCARD_UNTIL_GAP
} p5_modbus_rtu_stream_state_t;

typedef void (*p5_modbus_rtu_stream_consumer_t)(
    void *context,
    const uint8_t *frame,
    size_t frame_length,
    const p5_modbus_adu_view_t *view);

typedef struct
{
  uint32_t character_ticks;
  uint32_t t1_5_ticks;
  uint32_t t3_5_ticks;
  p5_modbus_rtu_stream_consumer_t consumer;
  void *consumer_context;
} p5_modbus_rtu_stream_config_t;

typedef struct
{
  uint32_t accepted_frames;
  uint32_t short_frames;
  uint32_t crc_mismatches;
  uint32_t inter_character_errors;
  uint32_t overlong_frames;
  uint32_t discarded_bytes;
  uint32_t partial_resets;
} p5_modbus_rtu_stream_counters_t;

typedef struct
{
  p5_modbus_rtu_stream_config_t config;
  p5_modbus_rtu_stream_state_t state;
  p5_modbus_rtu_stream_counters_t counters;
  uint8_t frame[P5_MODBUS_RTU_MAX_ADU_SIZE];
  size_t frame_length;
  uint32_t last_byte_end_ticks;
} p5_modbus_rtu_stream_t;

bool p5_modbus_rtu_stream_initialize(
    p5_modbus_rtu_stream_t *stream,
    const p5_modbus_rtu_stream_config_t *config);
void p5_modbus_rtu_stream_push_byte(p5_modbus_rtu_stream_t *stream,
                                    uint8_t byte,
                                    uint32_t byte_end_ticks);
void p5_modbus_rtu_stream_poll(p5_modbus_rtu_stream_t *stream,
                               uint32_t now_ticks);
void p5_modbus_rtu_stream_reset(p5_modbus_rtu_stream_t *stream);
bool p5_modbus_rtu_stream_has_partial_frame(
    const p5_modbus_rtu_stream_t *stream);
p5_modbus_rtu_stream_counters_t p5_modbus_rtu_stream_counters(
    const p5_modbus_rtu_stream_t *stream);

#endif
