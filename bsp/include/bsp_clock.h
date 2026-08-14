#ifndef BSP_CLOCK_H
#define BSP_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#define BSP_CLOCK_EXPECTED_SYSCLK_HZ UINT32_C(180000000)
#define BSP_CLOCK_EXPECTED_HCLK_HZ UINT32_C(180000000)
#define BSP_CLOCK_EXPECTED_PCLK1_HZ UINT32_C(45000000)
#define BSP_CLOCK_EXPECTED_PCLK2_HZ UINT32_C(90000000)
#define BSP_CLOCK_EXPECTED_HAL_TICK_QUANTUM_MS UINT32_C(1)

typedef struct
{
  uint32_t sysclk_hz;
  uint32_t hclk_hz;
  uint32_t pclk1_hz;
  uint32_t pclk2_hz;
  uint32_t hal_tick_quantum_ms;
} bsp_clock_profile_t;

uint32_t bsp_clock_tick_ms(void);
bool bsp_clock_cycle_counter_initialize(void);
bool bsp_clock_cycle_counter_is_ready(void);
uint32_t bsp_clock_cycle_now(void);
uint32_t bsp_clock_cycles_per_us(void);
bsp_clock_profile_t bsp_clock_get_profile(void);
bool bsp_clock_profile_is_expected(const bsp_clock_profile_t *profile);
uint32_t bsp_clock_elapsed_ms(uint32_t now_ms, uint32_t start_ms);
bool bsp_clock_interval_elapsed(uint32_t now_ms,
                                uint32_t start_ms,
                                uint32_t interval_ms);

#endif
