#include "p5_modbus_rtu_stream.h"

#include <string.h>

static uint32_t p5_stream_increment(uint32_t value)
{
  return (value == UINT32_MAX) ? UINT32_MAX : (value + 1U);
}

static void p5_stream_clear_frame(p5_modbus_rtu_stream_t *stream)
{
  stream->frame_length = 0U;
  stream->state = P5_MODBUS_STREAM_IDLE;
}

static void p5_stream_start_frame(p5_modbus_rtu_stream_t *stream,
                                  uint8_t byte,
                                  uint32_t byte_end_ticks)
{
  stream->frame[0] = byte;
  stream->frame_length = 1U;
  stream->last_byte_end_ticks = byte_end_ticks;
  stream->state = P5_MODBUS_STREAM_RECEIVING;
}

static void p5_stream_finalize(p5_modbus_rtu_stream_t *stream)
{
  p5_modbus_adu_view_t view;
  const p5_modbus_adu_status_t status = p5_modbus_rtu_adu_decode(
      stream->frame, stream->frame_length, &view);

  if (status == P5_MODBUS_ADU_OK)
  {
    stream->counters.accepted_frames =
        p5_stream_increment(stream->counters.accepted_frames);
    stream->config.consumer(stream->config.consumer_context,
                            stream->frame,
                            stream->frame_length,
                            &view);
  }
  else if (status == P5_MODBUS_ADU_FRAME_TOO_SHORT)
  {
    stream->counters.short_frames =
        p5_stream_increment(stream->counters.short_frames);
  }
  else
  {
    stream->counters.crc_mismatches =
        p5_stream_increment(stream->counters.crc_mismatches);
  }
  p5_stream_clear_frame(stream);
}

bool p5_modbus_rtu_stream_initialize(
    p5_modbus_rtu_stream_t *stream,
    const p5_modbus_rtu_stream_config_t *config)
{
  if ((stream == NULL) || (config == NULL) ||
      (config->character_ticks == 0U) || (config->t1_5_ticks == 0U) ||
      (config->t3_5_ticks <= config->t1_5_ticks) ||
      (config->consumer == NULL))
  {
    return false;
  }

  (void)memset(stream, 0, sizeof(*stream));
  stream->config = *config;
  stream->state = P5_MODBUS_STREAM_IDLE;
  return true;
}

void p5_modbus_rtu_stream_push_byte(p5_modbus_rtu_stream_t *stream,
                                    uint8_t byte,
                                    uint32_t byte_end_ticks)
{
  if (stream == NULL)
  {
    return;
  }

  if (stream->state == P5_MODBUS_STREAM_IDLE)
  {
    p5_stream_start_frame(stream, byte, byte_end_ticks);
    return;
  }

  const uint32_t end_to_end = byte_end_ticks - stream->last_byte_end_ticks;
  const uint32_t silence =
      (end_to_end > stream->config.character_ticks)
          ? (end_to_end - stream->config.character_ticks)
          : 0U;

  if (silence >= stream->config.t3_5_ticks)
  {
    if (stream->state == P5_MODBUS_STREAM_RECEIVING)
    {
      p5_stream_finalize(stream);
    }
    else
    {
      p5_stream_clear_frame(stream);
    }
    p5_stream_start_frame(stream, byte, byte_end_ticks);
    return;
  }

  if (stream->state == P5_MODBUS_STREAM_DISCARD_UNTIL_GAP)
  {
    stream->counters.discarded_bytes =
        p5_stream_increment(stream->counters.discarded_bytes);
    stream->last_byte_end_ticks = byte_end_ticks;
    return;
  }

  if (silence > stream->config.t1_5_ticks)
  {
    stream->counters.inter_character_errors =
        p5_stream_increment(stream->counters.inter_character_errors);
    stream->counters.discarded_bytes =
        p5_stream_increment(stream->counters.discarded_bytes);
    stream->frame_length = 0U;
    stream->last_byte_end_ticks = byte_end_ticks;
    stream->state = P5_MODBUS_STREAM_DISCARD_UNTIL_GAP;
    return;
  }

  if (stream->frame_length >= P5_MODBUS_RTU_MAX_ADU_SIZE)
  {
    stream->counters.overlong_frames =
        p5_stream_increment(stream->counters.overlong_frames);
    stream->counters.discarded_bytes =
        p5_stream_increment(stream->counters.discarded_bytes);
    stream->frame_length = 0U;
    stream->last_byte_end_ticks = byte_end_ticks;
    stream->state = P5_MODBUS_STREAM_DISCARD_UNTIL_GAP;
    return;
  }

  stream->frame[stream->frame_length] = byte;
  ++stream->frame_length;
  stream->last_byte_end_ticks = byte_end_ticks;
}

void p5_modbus_rtu_stream_poll(p5_modbus_rtu_stream_t *stream,
                               uint32_t now_ticks)
{
  if ((stream == NULL) || (stream->state == P5_MODBUS_STREAM_IDLE))
  {
    return;
  }

  if ((now_ticks - stream->last_byte_end_ticks) <
      stream->config.t3_5_ticks)
  {
    return;
  }

  if (stream->state == P5_MODBUS_STREAM_RECEIVING)
  {
    p5_stream_finalize(stream);
  }
  else
  {
    p5_stream_clear_frame(stream);
  }
}

void p5_modbus_rtu_stream_reset(p5_modbus_rtu_stream_t *stream)
{
  if (stream == NULL)
  {
    return;
  }
  if (stream->state != P5_MODBUS_STREAM_IDLE)
  {
    stream->counters.partial_resets =
        p5_stream_increment(stream->counters.partial_resets);
  }
  p5_stream_clear_frame(stream);
}

bool p5_modbus_rtu_stream_has_partial_frame(
    const p5_modbus_rtu_stream_t *stream)
{
  return (stream != NULL) &&
         (stream->state != P5_MODBUS_STREAM_IDLE);
}

p5_modbus_rtu_stream_counters_t p5_modbus_rtu_stream_counters(
    const p5_modbus_rtu_stream_t *stream)
{
  const p5_modbus_rtu_stream_counters_t empty = {0U, 0U, 0U, 0U, 0U, 0U, 0U};
  return (stream == NULL) ? empty : stream->counters;
}
