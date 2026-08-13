#include "app_boot.h"

#include <stdbool.h>

#include "app_device_probe.h"
#include "app_rs485_smoke.h"
#include "bsp_clock.h"
#include "bsp_rs485.h"
#include "stm32f4xx_hal.h"
#include "usart.h"

#ifndef P5_DEVICE_PROBE_SMOKE_ENABLE
#define P5_DEVICE_PROBE_SMOKE_ENABLE (0)
#endif

#define APP_BOOT_MARKER_REPEAT_COUNT (5U)
#define APP_BOOT_MARKER_INTERVAL_MS (1000U)
#define APP_BOOT_UART_TIMEOUT_MS (100U)
#define APP_HEARTBEAT_REPEAT_COUNT (5U)
#define APP_HEARTBEAT_INTERVAL_MS (1000U)

static uint8_t app_boot_marker[] = "P5 S2 T01 BOOT OK\r\n";
static uint8_t app_clock_marker[] =
    "P5 S2 T02 CLOCK SYS=180000000 HCLK=180000000 "
    "PCLK1=45000000 PCLK2=90000000 TICK=1MS\r\n";
static uint8_t app_heartbeat_marker[] = "P5 S2 T02 HEARTBEAT OK\r\n";

static uint32_t app_heartbeat_last_tick_ms;
static uint32_t app_heartbeat_count;
static bool app_heartbeat_enabled;

app_boot_status_t app_boot_initialize(void)
{
  for (uint32_t attempt = 0U; attempt < APP_BOOT_MARKER_REPEAT_COUNT; ++attempt)
  {
    if (HAL_UART_Transmit(&huart2,
                          app_boot_marker,
                          (uint16_t)(sizeof(app_boot_marker) - 1U),
                          APP_BOOT_UART_TIMEOUT_MS) != HAL_OK)
    {
      return APP_BOOT_ERROR;
    }

    if ((attempt + 1U) < APP_BOOT_MARKER_REPEAT_COUNT)
    {
      HAL_Delay(APP_BOOT_MARKER_INTERVAL_MS);
    }
  }

  const bsp_clock_profile_t profile = bsp_clock_get_profile();
  if (!bsp_clock_profile_is_expected(&profile))
  {
    return APP_BOOT_ERROR;
  }

  if (HAL_UART_Transmit(&huart2,
                        app_clock_marker,
                        (uint16_t)(sizeof(app_clock_marker) - 1U),
                        APP_BOOT_UART_TIMEOUT_MS) != HAL_OK)
  {
    return APP_BOOT_ERROR;
  }

  app_heartbeat_last_tick_ms = bsp_clock_tick_ms();
  app_heartbeat_count = 0U;
  app_heartbeat_enabled = true;

  if (bsp_rs485_initialize() != BSP_RS485_RESULT_OK)
  {
    return APP_BOOT_ERROR;
  }
  app_rs485_smoke_initialize();

  app_device_probe_initialize();
#if P5_DEVICE_PROBE_SMOKE_ENABLE
  app_device_probe_run_once();
#endif

  return APP_BOOT_OK;
}

void app_boot_idle(void)
{
  (void)bsp_rs485_poll();
  app_rs485_smoke_poll();

  if (app_heartbeat_enabled)
  {
    const uint32_t now_ms = bsp_clock_tick_ms();
    if (bsp_clock_interval_elapsed(now_ms,
                                   app_heartbeat_last_tick_ms,
                                   APP_HEARTBEAT_INTERVAL_MS))
    {
      (void)HAL_UART_Transmit(&huart2,
                              app_heartbeat_marker,
                              (uint16_t)(sizeof(app_heartbeat_marker) - 1U),
                              APP_BOOT_UART_TIMEOUT_MS);

      ++app_heartbeat_count;
      app_heartbeat_last_tick_ms = now_ms;
      app_heartbeat_enabled = app_heartbeat_count < APP_HEARTBEAT_REPEAT_COUNT;
    }
  }

  __WFI();
}
