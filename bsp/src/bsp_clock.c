#include "bsp_clock.h"

#include "stm32f4xx_hal.h"

uint32_t bsp_clock_tick_ms(void)
{
  return HAL_GetTick();
}
