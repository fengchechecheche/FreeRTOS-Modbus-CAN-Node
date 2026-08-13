#include "app_boot.h"

#include "stm32f4xx_hal.h"
#include "usart.h"

#define APP_BOOT_MARKER_REPEAT_COUNT (5U)
#define APP_BOOT_MARKER_INTERVAL_MS (1000U)
#define APP_BOOT_UART_TIMEOUT_MS (100U)

static uint8_t app_boot_marker[] = "P5 S2 T01 BOOT OK\r\n";

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

  return APP_BOOT_OK;
}

void app_boot_idle(void)
{
  __WFI();
}
