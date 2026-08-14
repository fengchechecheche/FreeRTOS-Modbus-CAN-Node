#include "p5_modbus_rtu_timing.h"

#include <limits.h>
#include <stddef.h>

#define P5_MODBUS_BITS_PER_8E1_CHARACTER (11U)
#define P5_MODBUS_TIMING_FORMULA_MAX_BAUD (19200U)
#define P5_MODBUS_FIXED_T1_5_US (750U)
#define P5_MODBUS_FIXED_T3_5_US (1750U)

static bool p5_modbus_ceil_divide_u64(uint64_t numerator,
                                      uint64_t denominator,
                                      uint32_t *result_out)
{
  if ((denominator == 0U) || (result_out == NULL))
  {
    return false;
  }

  uint64_t result = numerator / denominator;
  if ((numerator % denominator) != 0U)
  {
    ++result;
  }
  if (result > UINT32_MAX)
  {
    return false;
  }

  *result_out = (uint32_t)result;
  return true;
}

bool p5_modbus_rtu_timing_8e1(uint32_t baud,
                               p5_modbus_rtu_timing_t *timing_out)
{
  if ((baud == 0U) || (timing_out == NULL))
  {
    return false;
  }

  p5_modbus_rtu_timing_t timing = {0U, 0U, 0U};
  const uint64_t baud_u64 = (uint64_t)baud;
  if (!p5_modbus_ceil_divide_u64(
          (uint64_t)P5_MODBUS_BITS_PER_8E1_CHARACTER * UINT64_C(1000000),
          baud_u64,
          &timing.character_us))
  {
    return false;
  }

  if (baud <= P5_MODBUS_TIMING_FORMULA_MAX_BAUD)
  {
    const uint64_t scaled_baud = UINT64_C(10) * baud_u64;
    if (!p5_modbus_ceil_divide_u64(
            UINT64_C(15) *
                (uint64_t)P5_MODBUS_BITS_PER_8E1_CHARACTER *
                UINT64_C(1000000),
            scaled_baud,
            &timing.inter_character_us) ||
        !p5_modbus_ceil_divide_u64(
            UINT64_C(35) *
                (uint64_t)P5_MODBUS_BITS_PER_8E1_CHARACTER *
                UINT64_C(1000000),
            scaled_baud,
            &timing.inter_frame_us))
    {
      return false;
    }
  }
  else
  {
    timing.inter_character_us = P5_MODBUS_FIXED_T1_5_US;
    timing.inter_frame_us = P5_MODBUS_FIXED_T3_5_US;
  }

  *timing_out = timing;
  return true;
}
