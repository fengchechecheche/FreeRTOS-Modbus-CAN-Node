#include "bsp_clock.h"

#include "stm32f4xx_hal.h"

uint32_t bsp_clock_tick_ms(void)
{
  return HAL_GetTick();
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
