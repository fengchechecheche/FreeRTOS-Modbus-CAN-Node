#include "app_rs485_smoke.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_rs485_smoke_logic.h"
#include "bsp_rs485.h"

#ifndef P5_RS485_LOOPBACK_SMOKE_ENABLE
#define P5_RS485_LOOPBACK_SMOKE_ENABLE (0)
#endif

#define APP_RS485_LOOPBACK_REPEAT_COUNT (3U)

static uint8_t app_rs485_rx_frame[BSP_RS485_MAX_FRAME_SIZE];
static uint8_t app_rs485_response[BSP_RS485_MAX_FRAME_SIZE];

#if P5_RS485_LOOPBACK_SMOKE_ENABLE
static uint32_t app_rs485_loopback_completed;
static bool app_rs485_loopback_waiting;
#endif

void app_rs485_smoke_initialize(void)
{
#if P5_RS485_LOOPBACK_SMOKE_ENABLE
  app_rs485_loopback_completed = 0U;
  app_rs485_loopback_waiting = false;
#endif
}

void app_rs485_smoke_poll(void)
{
  size_t received_length = 0U;
  if (bsp_rs485_take_received(app_rs485_rx_frame,
                              sizeof(app_rs485_rx_frame),
                              &received_length))
  {
    size_t response_length = 0U;
    const bool request_matches = app_rs485_smoke_build_response(
        app_rs485_rx_frame,
        received_length,
        app_rs485_response,
        sizeof(app_rs485_response),
        &response_length);

#if P5_RS485_LOOPBACK_SMOKE_ENABLE
    if (app_rs485_loopback_waiting && request_matches)
    {
      ++app_rs485_loopback_completed;
      app_rs485_loopback_waiting = false;
    }
#else
    if (request_matches)
    {
      (void)bsp_rs485_send(app_rs485_response, response_length);
    }
#endif
  }

#if P5_RS485_LOOPBACK_SMOKE_ENABLE
  if (!app_rs485_loopback_waiting &&
      (app_rs485_loopback_completed < APP_RS485_LOOPBACK_REPEAT_COUNT) &&
      !bsp_rs485_is_busy())
  {
    size_t request_length = 0U;
    const uint8_t *request = app_rs485_smoke_request(&request_length);
    app_rs485_loopback_waiting =
        bsp_rs485_send(request, request_length) == BSP_RS485_RESULT_OK;
  }
#endif
}
