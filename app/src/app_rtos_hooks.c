#include "app_rtos.h"

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

static StaticTask_t app_rtos_idle_task_control;
static StackType_t app_rtos_idle_task_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **idle_task_control,
                                   StackType_t **idle_task_stack,
                                   uint32_t *idle_task_stack_size)
{
  *idle_task_control = &app_rtos_idle_task_control;
  *idle_task_stack = app_rtos_idle_task_stack;
  *idle_task_stack_size = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
  (void)task;
  (void)task_name;
  app_rtos_fail_stop(APP_RTOS_FAULT_STACK_OVERFLOW);
}

void app_rtos_assert_failed(const char *file, uint32_t line)
{
  (void)file;
  app_rtos_fail_stop(APP_RTOS_FAULT_ASSERT | (line & UINT32_C(0x000f)));
}
