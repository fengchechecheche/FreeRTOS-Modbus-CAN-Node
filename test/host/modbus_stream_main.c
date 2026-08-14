#include "p5_modbus_rtu_stream.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define CHECK(condition)          \
  do                              \
  {                               \
    if (!(condition))             \
    {                             \
      return EXIT_FAILURE;        \
    }                             \
  } while (false)

enum
{
  TEST_CHARACTER_TICKS = 10,
  TEST_T1_5_TICKS = 15,
  TEST_T3_5_TICKS = 35
};

typedef struct
{
  uint32_t calls;
  size_t last_length;
  uint8_t last_address;
  uint8_t last_function;
} consumer_trace_t;

static void consume(void *context,
                    const uint8_t *frame,
                    size_t frame_length,
                    const p5_modbus_adu_view_t *view)
{
  consumer_trace_t *trace = context;
  (void)frame;
  ++trace->calls;
  trace->last_length = frame_length;
  trace->last_address = view->address;
  trace->last_function = view->function;
}

static bool initialize(p5_modbus_rtu_stream_t *stream,
                       consumer_trace_t *trace)
{
  const p5_modbus_rtu_stream_config_t config = {
      .character_ticks = TEST_CHARACTER_TICKS,
      .t1_5_ticks = TEST_T1_5_TICKS,
      .t3_5_ticks = TEST_T3_5_TICKS,
      .consumer = consume,
      .consumer_context = trace,
  };
  return p5_modbus_rtu_stream_initialize(stream, &config);
}

static size_t make_request(uint8_t address, uint8_t *frame)
{
  const uint8_t data[] = {0x00U, 0x01U, 0x00U, 0x02U};
  size_t length = 0U;
  if (p5_modbus_rtu_adu_encode(address,
                                0x03U,
                                data,
                                sizeof(data),
                                frame,
                                P5_MODBUS_RTU_MAX_ADU_SIZE,
                                &length) != P5_MODBUS_ADU_OK)
  {
    return 0U;
  }
  return length;
}

static uint32_t push_contiguous(p5_modbus_rtu_stream_t *stream,
                                const uint8_t *frame,
                                size_t length,
                                uint32_t first_end)
{
  uint32_t timestamp = first_end;
  for (size_t index = 0U; index < length; ++index)
  {
    p5_modbus_rtu_stream_push_byte(stream, frame[index], timestamp);
    timestamp += TEST_CHARACTER_TICKS;
  }
  return timestamp - TEST_CHARACTER_TICKS;
}

static int test_valid_frame_and_wrap(void)
{
  p5_modbus_rtu_stream_t stream;
  consumer_trace_t trace = {0U, 0U, 0U, 0U};
  uint8_t frame[P5_MODBUS_RTU_MAX_ADU_SIZE];
  const size_t length = make_request(0x11U, frame);
  CHECK(length == 8U);
  CHECK(initialize(&stream, &trace));

  const uint32_t last = push_contiguous(
      &stream, frame, length, UINT32_C(0xffffffe0));
  CHECK(p5_modbus_rtu_stream_has_partial_frame(&stream));
  p5_modbus_rtu_stream_poll(&stream, last + TEST_T3_5_TICKS);
  CHECK(trace.calls == 1U);
  CHECK(trace.last_length == length);
  CHECK(trace.last_address == 0x11U);
  CHECK(trace.last_function == 0x03U);
  CHECK(!p5_modbus_rtu_stream_has_partial_frame(&stream));
  return EXIT_SUCCESS;
}

static int test_t1_5_boundary_and_violation(void)
{
  uint8_t frame[P5_MODBUS_RTU_MAX_ADU_SIZE];
  const size_t length = make_request(0x22U, frame);
  p5_modbus_rtu_stream_t stream;
  consumer_trace_t trace = {0U, 0U, 0U, 0U};
  CHECK(initialize(&stream, &trace));

  uint32_t timestamp = 100U;
  for (size_t index = 0U; index < length; ++index)
  {
    p5_modbus_rtu_stream_push_byte(&stream, frame[index], timestamp);
    timestamp += (index == 2U)
                     ? (TEST_CHARACTER_TICKS + TEST_T1_5_TICKS)
                     : TEST_CHARACTER_TICKS;
  }
  p5_modbus_rtu_stream_poll(
      &stream, timestamp - TEST_CHARACTER_TICKS + TEST_T3_5_TICKS);
  CHECK(trace.calls == 1U);

  CHECK(initialize(&stream, &trace));
  timestamp = 1000U;
  for (size_t index = 0U; index < length; ++index)
  {
    p5_modbus_rtu_stream_push_byte(&stream, frame[index], timestamp);
    timestamp += (index == 2U)
                     ? (TEST_CHARACTER_TICKS + TEST_T1_5_TICKS + 1U)
                     : TEST_CHARACTER_TICKS;
  }
  p5_modbus_rtu_stream_poll(
      &stream, timestamp - TEST_CHARACTER_TICKS + TEST_T3_5_TICKS);
  CHECK(trace.calls == 1U);
  CHECK(p5_modbus_rtu_stream_counters(&stream).inter_character_errors == 1U);
  return EXIT_SUCCESS;
}

static int test_gap_splits_frames(void)
{
  p5_modbus_rtu_stream_t stream;
  consumer_trace_t trace = {0U, 0U, 0U, 0U};
  uint8_t first[P5_MODBUS_RTU_MAX_ADU_SIZE];
  uint8_t second[P5_MODBUS_RTU_MAX_ADU_SIZE];
  const size_t first_length = make_request(0x31U, first);
  const size_t second_length = make_request(0x32U, second);
  CHECK(initialize(&stream, &trace));

  const uint32_t first_last = push_contiguous(&stream, first, first_length, 10U);
  const uint32_t second_first = first_last + TEST_CHARACTER_TICKS +
                                TEST_T3_5_TICKS;
  const uint32_t second_last = push_contiguous(
      &stream, second, second_length, second_first);
  CHECK(trace.calls == 1U);
  p5_modbus_rtu_stream_poll(&stream, second_last + TEST_T3_5_TICKS);
  CHECK(trace.calls == 2U);
  CHECK(trace.last_address == 0x32U);
  return EXIT_SUCCESS;
}

static int test_short_crc_and_overlong_are_bounded(void)
{
  p5_modbus_rtu_stream_t stream;
  consumer_trace_t trace = {0U, 0U, 0U, 0U};
  uint8_t frame[P5_MODBUS_RTU_MAX_ADU_SIZE];
  const size_t length = make_request(0x44U, frame);
  CHECK(initialize(&stream, &trace));

  p5_modbus_rtu_stream_push_byte(&stream, 0x01U, 10U);
  p5_modbus_rtu_stream_push_byte(&stream, 0x03U, 20U);
  p5_modbus_rtu_stream_poll(&stream, 20U + TEST_T3_5_TICKS);
  CHECK(p5_modbus_rtu_stream_counters(&stream).short_frames == 1U);

  frame[length - 1U] ^= 0x01U;
  const uint32_t crc_last = push_contiguous(&stream, frame, length, 100U);
  p5_modbus_rtu_stream_poll(&stream, crc_last + TEST_T3_5_TICKS);
  CHECK(p5_modbus_rtu_stream_counters(&stream).crc_mismatches == 1U);

  uint32_t timestamp = 1000U;
  for (size_t index = 0U; index <= P5_MODBUS_RTU_MAX_ADU_SIZE; ++index)
  {
    p5_modbus_rtu_stream_push_byte(&stream, 0x00U, timestamp);
    timestamp += TEST_CHARACTER_TICKS;
  }
  CHECK(stream.state == P5_MODBUS_STREAM_DISCARD_UNTIL_GAP);
  CHECK(p5_modbus_rtu_stream_counters(&stream).overlong_frames == 1U);
  p5_modbus_rtu_stream_poll(
      &stream, timestamp - TEST_CHARACTER_TICKS + TEST_T3_5_TICKS);
  CHECK(stream.state == P5_MODBUS_STREAM_IDLE);
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(test_valid_frame_and_wrap() == EXIT_SUCCESS);
  CHECK(test_t1_5_boundary_and_violation() == EXIT_SUCCESS);
  CHECK(test_gap_splits_frames() == EXIT_SUCCESS);
  CHECK(test_short_crc_and_overlong_are_bounded() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
