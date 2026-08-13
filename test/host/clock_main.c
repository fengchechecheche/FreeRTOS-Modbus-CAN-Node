#include "bsp_clock.h"

#include <stdint.h>
#include <stdlib.h>

static int expect_true(bool condition)
{
  return condition ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(void)
{
  if (bsp_clock_elapsed_ms(UINT32_C(1060), UINT32_C(1000)) != UINT32_C(60))
  {
    return EXIT_FAILURE;
  }

  if (expect_true(!bsp_clock_interval_elapsed(UINT32_C(1999),
                                               UINT32_C(1000),
                                               UINT32_C(1000))) != EXIT_SUCCESS)
  {
    return EXIT_FAILURE;
  }

  if (expect_true(bsp_clock_interval_elapsed(UINT32_C(2000),
                                              UINT32_C(1000),
                                              UINT32_C(1000))) != EXIT_SUCCESS)
  {
    return EXIT_FAILURE;
  }

  const uint32_t wrap_start_ms = UINT32_C(0xfffffff0);
  const uint32_t wrap_now_ms = UINT32_C(0x00000020);
  if (bsp_clock_elapsed_ms(wrap_now_ms, wrap_start_ms) != UINT32_C(48))
  {
    return EXIT_FAILURE;
  }

  if (expect_true(bsp_clock_interval_elapsed(wrap_now_ms,
                                              wrap_start_ms,
                                              UINT32_C(48))) != EXIT_SUCCESS)
  {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
