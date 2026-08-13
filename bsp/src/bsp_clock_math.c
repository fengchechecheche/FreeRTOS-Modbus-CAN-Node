#include "bsp_clock.h"

uint32_t bsp_clock_elapsed_ms(uint32_t now_ms, uint32_t start_ms)
{
  return now_ms - start_ms;
}

bool bsp_clock_interval_elapsed(uint32_t now_ms,
                                uint32_t start_ms,
                                uint32_t interval_ms)
{
  return bsp_clock_elapsed_ms(now_ms, start_ms) >= interval_ms;
}
