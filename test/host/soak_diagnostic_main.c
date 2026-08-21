#include "app_soak_diagnostic.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
  app_soak_diagnostic_snapshot_t snapshot = {0};
  char line[APP_SOAK_DIAGNOSTIC_LINE_CAPACITY];
  size_t length = 0U;

  snapshot.now_ms = 60000U;
  snapshot.boot_count = 3U;
  snapshot.queue_depth = 8U;
  snapshot.can_capacity = 14U;
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    snapshot.task[index].release = (uint32_t)(index + 1U);
    snapshot.task[index].configured_words = 256U;
    snapshot.task[index].minimum_free_words = 128U;
    snapshot.task[index].measured = true;
  }
  for (size_t index = 0U; index < APP_SOAK_DIAGNOSTIC_SOURCE_COUNT; ++index)
  {
    snapshot.sensor[index].state = 1U;
    snapshot.sensor[index].sequence = (uint32_t)(100U + index);
  }

  if (!app_soak_diagnostic_format(&snapshot, line, sizeof(line), &length) ||
      (length == 0U) || (strstr(line, "P5DIAG1 v=1 t=60000 boot=3") == NULL) ||
      (strstr(line, "q=0/0/8/0/0") == NULL) ||
      (strstr(line, "can=0/0/14/0/0/0/0/0") == NULL) ||
      (line[length - 2U] != '\r') || (line[length - 1U] != '\n'))
  {
    return 1;
  }

  if (app_soak_diagnostic_format(&snapshot, line, 16U, &length) ||
      (length != 0U) || (line[0] != '\0'))
  {
    return 2;
  }
  if (app_soak_diagnostic_format(NULL, line, sizeof(line), &length))
  {
    return 3;
  }

  memset(&snapshot, 0xFF, sizeof(snapshot));
  snapshot.reset_loop = true;
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    snapshot.task[index].measured = true;
  }
  if (!app_soak_diagnostic_format(&snapshot, line, sizeof(line), &length) ||
      (length >= sizeof(line)) ||
      (strstr(line, "t=4294967295 boot=4294967295") == NULL) ||
      (strstr(line, "sm=1F") == NULL) ||
      (strstr(line, "h=4294967295/FFFFFFFF/FFFFFFFF/") == NULL) ||
      (strstr(line, "rst=4294967295/FFFFFFFF/1") == NULL) ||
      (strstr(line, "can=4294967295/4294967295/") == NULL) ||
      (line[length - 2U] != '\r') || (line[length - 1U] != '\n'))
  {
    return 4;
  }

  puts("P5 SOAK DIAGNOSTIC FORMAT: PASS");
  return 0;
}
