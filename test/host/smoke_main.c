#include <stdint.h>
#include <stdlib.h>

int main(void) {
  const uint32_t sample = UINT32_C(0x446);
  const char *const force_failure = getenv("P5_SMOKE_FORCE_FAIL");

  if (force_failure != NULL && force_failure[0] == '1') {
    return EXIT_FAILURE;
  }

  return sample == UINT32_C(0x446) ? EXIT_SUCCESS : EXIT_FAILURE;
}
