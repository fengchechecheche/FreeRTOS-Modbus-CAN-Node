#ifndef APP_TASK_MODEL_H
#define APP_TASK_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
  APP_TASK_PROTOCOL = 0,
  APP_TASK_ACQUISITION,
  APP_TASK_CAN,
  APP_TASK_HEALTH,
  APP_TASK_DIAGNOSTIC,
  APP_TASK_COUNT
} app_task_id_t;

typedef struct
{
  app_task_id_t id;
  const char *name;
  uint8_t priority;
  uint32_t period_ms;
  uint32_t deadline_ms;
  uint32_t execution_budget_ms;
} app_task_contract_t;

typedef struct
{
  uint32_t next_release_tick;
  uint32_t release_count;
  uint32_t missed_release_count;
  uint32_t deadline_miss_count;
  uint32_t budget_overrun_count;
} app_task_runtime_t;

size_t app_task_model_count(void);
const app_task_contract_t *app_task_model_contract(app_task_id_t id);
bool app_task_model_is_valid(void);
void app_task_runtime_initialize(app_task_runtime_t *runtime,
                                 uint32_t first_release_tick);
bool app_task_runtime_record_cycle(app_task_runtime_t *runtime,
                                   const app_task_contract_t *contract,
                                   uint32_t actual_start_tick,
                                   uint32_t actual_finish_tick);

#endif
