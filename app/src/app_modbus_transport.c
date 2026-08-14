#include "app_modbus_transport.h"

#include <stddef.h>

#include "bsp_clock.h"
#include "bsp_rs485.h"
#include "p5_modbus_rtu_timing.h"

#define APP_MODBUS_BAUD_RATE UINT32_C(19200)

static p5_modbus_rtu_stream_t app_modbus_stream;
static app_modbus_transport_diagnostics_t app_modbus_diagnostics;
static uint32_t app_modbus_character_cycles;

static uint32_t app_modbus_increment(uint32_t value)
{
  return (value == UINT32_MAX) ? UINT32_MAX : (value + 1U);
}

static uint32_t app_modbus_add(uint32_t value, uint32_t increment)
{
  return (increment > (UINT32_MAX - value)) ? UINT32_MAX
                                             : (value + increment);
}

static void app_modbus_consume(void *context,
                               const uint8_t *frame,
                               size_t frame_length,
                               const p5_modbus_adu_view_t *view)
{
  (void)context;
  (void)frame;
  (void)frame_length;
  (void)view;
  app_modbus_diagnostics.unhandled_valid_frames = app_modbus_increment(
      app_modbus_diagnostics.unhandled_valid_frames);
}

bool app_modbus_transport_initialize(void)
{
  const uint32_t cycles_per_us = bsp_clock_cycles_per_us();
  p5_modbus_rtu_timing_t timing;
  if ((cycles_per_us == 0U) ||
      !p5_modbus_rtu_timing_8e1(APP_MODBUS_BAUD_RATE, &timing))
  {
    return false;
  }

  app_modbus_character_cycles = timing.character_us * cycles_per_us;
  const p5_modbus_rtu_stream_config_t config = {
      .character_ticks = app_modbus_character_cycles,
      .t1_5_ticks = timing.inter_character_us * cycles_per_us,
      .t3_5_ticks = timing.inter_frame_us * cycles_per_us,
      .consumer = app_modbus_consume,
      .consumer_context = NULL,
  };
  app_modbus_diagnostics = (app_modbus_transport_diagnostics_t){0};
  return p5_modbus_rtu_stream_initialize(&app_modbus_stream, &config);
}

void app_modbus_transport_service_received(void)
{
  uint8_t chunk[BSP_RS485_RX_DMA_CHUNK_SIZE];
  size_t length = 0U;
  bsp_rs485_rx_chunk_info_t info;
  if (!bsp_rs485_take_received_chunk(chunk,
                                     sizeof(chunk),
                                     &length,
                                     &info))
  {
    return;
  }

  if ((length == 0U) || (length > sizeof(chunk)) ||
      ((info.kind != BSP_RS485_RX_EVENT_IDLE) &&
       (info.kind != BSP_RS485_RX_EVENT_DMA_COMPLETE)))
  {
    app_modbus_diagnostics.invalid_chunks = app_modbus_increment(
        app_modbus_diagnostics.invalid_chunks);
    p5_modbus_rtu_stream_reset(&app_modbus_stream);
    return;
  }

  uint32_t last_byte_end = info.captured_cycles;
  if (info.kind == BSP_RS485_RX_EVENT_IDLE)
  {
    last_byte_end -= app_modbus_character_cycles;
    app_modbus_diagnostics.idle_chunks = app_modbus_increment(
        app_modbus_diagnostics.idle_chunks);
  }
  else
  {
    app_modbus_diagnostics.dma_complete_chunks = app_modbus_increment(
        app_modbus_diagnostics.dma_complete_chunks);
  }

  const uint32_t first_byte_end =
      last_byte_end - ((uint32_t)(length - 1U) * app_modbus_character_cycles);
  for (size_t index = 0U; index < length; ++index)
  {
    p5_modbus_rtu_stream_push_byte(
        &app_modbus_stream,
        chunk[index],
        first_byte_end + ((uint32_t)index * app_modbus_character_cycles));
  }

  app_modbus_diagnostics.rx_chunks = app_modbus_increment(
      app_modbus_diagnostics.rx_chunks);
  app_modbus_diagnostics.rx_bytes = app_modbus_add(
      app_modbus_diagnostics.rx_bytes, (uint32_t)length);
}

void app_modbus_transport_poll(void)
{
  p5_modbus_rtu_stream_poll(&app_modbus_stream, bsp_clock_cycle_now());
}

void app_modbus_transport_reset_partial(void)
{
  p5_modbus_rtu_stream_reset(&app_modbus_stream);
}

bool app_modbus_transport_has_partial_frame(void)
{
  return p5_modbus_rtu_stream_has_partial_frame(&app_modbus_stream);
}

app_modbus_transport_diagnostics_t app_modbus_transport_get_diagnostics(void)
{
  app_modbus_transport_diagnostics_t diagnostics = app_modbus_diagnostics;
  diagnostics.stream = p5_modbus_rtu_stream_counters(&app_modbus_stream);
  return diagnostics;
}
