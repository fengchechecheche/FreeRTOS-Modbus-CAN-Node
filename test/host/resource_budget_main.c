#include <assert.h>

#include "app_resource_budget.h"

_Static_assert(APP_RESOURCE_PROTOCOL_STACK_WORDS >=
                   APP_RESOURCE_IDLE_STACK_WORDS,
               "protocol task stack must meet the minimum");
_Static_assert(APP_RESOURCE_ACQUISITION_STACK_WORDS >=
                   APP_RESOURCE_IDLE_STACK_WORDS,
               "acquisition task stack must meet the minimum");
_Static_assert(APP_RESOURCE_CAN_STACK_WORDS >= APP_RESOURCE_IDLE_STACK_WORDS,
               "CAN task stack must meet the minimum");
_Static_assert(APP_RESOURCE_HEALTH_STACK_WORDS >=
                   APP_RESOURCE_IDLE_STACK_WORDS,
               "health task stack must meet the minimum");
_Static_assert(APP_RESOURCE_DIAGNOSTIC_STACK_WORDS >=
                   APP_RESOURCE_IDLE_STACK_WORDS,
               "diagnostic task stack must meet the minimum");

int main(void)
{
  assert(APP_RESOURCE_STACK_WORD_BYTES == 4U);
  assert(APP_RESOURCE_TASK_STACK_WORDS_TOTAL == 1280U);
  assert(APP_RESOURCE_TASK_STACK_BYTES_TOTAL == 5120U);
  assert(APP_RESOURCE_EXPLICIT_STACK_BYTES_TOTAL == 6656U);
  assert(APP_RESOURCE_LINKER_HEAP_BYTES == 0U);
  assert(APP_RESOURCE_MSP_STACK_BYTES == 1024U);
  assert(APP_RESOURCE_FLASH_LIMIT_BYTES == (384U * 1024U));
  assert(APP_RESOURCE_STATIC_RAM_LIMIT_BYTES == (96U * 1024U));
  assert(APP_RESOURCE_EXPLICIT_STACK_BYTES_TOTAL <
         APP_RESOURCE_STATIC_RAM_LIMIT_BYTES);

  return 0;
}
