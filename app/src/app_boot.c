#include "app_boot.h"

#include "stm32f4xx_hal.h"

app_boot_status_t app_boot_initialize(void)
{
  return APP_BOOT_OK;
}

void app_boot_idle(void)
{
  __WFI();
}
