#include "bsp_clock.h"

#include "stm32f4xx_hal.h"

static bool bsp_clock_cycle_counter_ready;
static uint32_t bsp_clock_cycle_counter_cycles_per_us;

uint32_t bsp_clock_tick_ms(void)
{
  return HAL_GetTick();
}

bool bsp_clock_cycle_counter_initialize(void)
{
  const uint32_t hclk_hz = HAL_RCC_GetHCLKFreq();
  bsp_clock_cycle_counter_ready = false;
  bsp_clock_cycle_counter_cycles_per_us = 0U;
  if ((hclk_hz == 0U) || ((hclk_hz % UINT32_C(1000000)) != 0U))
  {
    return false;
  }

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  if ((DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) != 0U)
  {
    return false;
  }

  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)
  {
    return false;
  }

  bsp_clock_cycle_counter_cycles_per_us = hclk_hz / UINT32_C(1000000);
  bsp_clock_cycle_counter_ready = true;
  return true;
}

bool bsp_clock_cycle_counter_is_ready(void)
{
  return bsp_clock_cycle_counter_ready;
}

uint32_t bsp_clock_cycle_now(void)
{
  return bsp_clock_cycle_counter_ready ? DWT->CYCCNT : 0U;
}

uint32_t bsp_clock_cycles_per_us(void)
{
  return bsp_clock_cycle_counter_ready
             ? bsp_clock_cycle_counter_cycles_per_us
             : 0U;
}

bsp_clock_profile_t bsp_clock_get_profile(void)
{
  const bsp_clock_profile_t profile = {
      .sysclk_hz = HAL_RCC_GetSysClockFreq(),
      .hclk_hz = HAL_RCC_GetHCLKFreq(),
      .pclk1_hz = HAL_RCC_GetPCLK1Freq(),
      .pclk2_hz = HAL_RCC_GetPCLK2Freq(),
      .hal_tick_quantum_ms = (uint32_t)HAL_GetTickFreq(),
  };

  return profile;
}

bool bsp_clock_profile_is_expected(const bsp_clock_profile_t *profile)
{
  if (profile == NULL)
  {
    return false;
  }

  return (profile->sysclk_hz == BSP_CLOCK_EXPECTED_SYSCLK_HZ) &&
         (profile->hclk_hz == BSP_CLOCK_EXPECTED_HCLK_HZ) &&
         (profile->pclk1_hz == BSP_CLOCK_EXPECTED_PCLK1_HZ) &&
         (profile->pclk2_hz == BSP_CLOCK_EXPECTED_PCLK2_HZ) &&
         (profile->hal_tick_quantum_ms == BSP_CLOCK_EXPECTED_HAL_TICK_QUANTUM_MS);
}
