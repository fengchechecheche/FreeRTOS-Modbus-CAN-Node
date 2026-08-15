#include "bsp_watchdog.h"

#include "iwdg.h"
#include "stm32f4xx_hal.h"

void bsp_watchdog_enable_debug_freeze(void)
{
  __HAL_DBGMCU_FREEZE_IWDG();
}

bool bsp_watchdog_refresh(void)
{
  return HAL_IWDG_Refresh(&hiwdg) == HAL_OK;
}
