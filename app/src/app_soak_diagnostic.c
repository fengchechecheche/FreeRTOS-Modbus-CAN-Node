#include "app_soak_diagnostic.h"

#include <stdio.h>

bool app_soak_diagnostic_format(
    const app_soak_diagnostic_snapshot_t *snapshot,
    char *buffer,
    size_t capacity,
    size_t *length)
{
  if ((snapshot == NULL) || (buffer == NULL) || (capacity == 0U) ||
      (length == NULL))
  {
    return false;
  }

  const int written = snprintf(
      buffer,
      capacity,
      "P5DIAG1 v=1 t=%lu boot=%lu "
      "tr=%lu/%lu/%lu/%lu/%lu "
      "tm=%lu/%lu/%lu/%lu/%lu "
      "td=%lu/%lu/%lu/%lu/%lu "
      "tb=%lu/%lu/%lu/%lu/%lu "
      "tc=%lu/%lu/%lu/%lu/%lu "
      "ts=%lu/%lu/%lu/%lu/%lu sm=%02X "
      "q=%lu/%lu/%lu/%lu/%lu "
      "h=%lu/%08lX/%08lX/%lu/%lu "
      "rst=%lu/%08lX/%u "
      "rs=%lu/%lu "
      "can=%lu/%lu/%lu/%lu/%lu/%lu/%lu/%lu "
      "sns=%lu,%lu,%lu,%lu;%lu,%lu,%lu,%lu;"
      "%lu,%lu,%lu,%lu;%lu,%lu,%lu,%lu\r\n",
      (unsigned long)snapshot->now_ms,
      (unsigned long)snapshot->boot_count,
      (unsigned long)snapshot->task[0].release,
      (unsigned long)snapshot->task[1].release,
      (unsigned long)snapshot->task[2].release,
      (unsigned long)snapshot->task[3].release,
      (unsigned long)snapshot->task[4].release,
      (unsigned long)snapshot->task[0].missed,
      (unsigned long)snapshot->task[1].missed,
      (unsigned long)snapshot->task[2].missed,
      (unsigned long)snapshot->task[3].missed,
      (unsigned long)snapshot->task[4].missed,
      (unsigned long)snapshot->task[0].deadline_miss,
      (unsigned long)snapshot->task[1].deadline_miss,
      (unsigned long)snapshot->task[2].deadline_miss,
      (unsigned long)snapshot->task[3].deadline_miss,
      (unsigned long)snapshot->task[4].deadline_miss,
      (unsigned long)snapshot->task[0].budget_overrun,
      (unsigned long)snapshot->task[1].budget_overrun,
      (unsigned long)snapshot->task[2].budget_overrun,
      (unsigned long)snapshot->task[3].budget_overrun,
      (unsigned long)snapshot->task[4].budget_overrun,
      (unsigned long)snapshot->task[0].configured_words,
      (unsigned long)snapshot->task[1].configured_words,
      (unsigned long)snapshot->task[2].configured_words,
      (unsigned long)snapshot->task[3].configured_words,
      (unsigned long)snapshot->task[4].configured_words,
      (unsigned long)snapshot->task[0].minimum_free_words,
      (unsigned long)snapshot->task[1].minimum_free_words,
      (unsigned long)snapshot->task[2].minimum_free_words,
      (unsigned long)snapshot->task[3].minimum_free_words,
      (unsigned long)snapshot->task[4].minimum_free_words,
      (unsigned int)((snapshot->task[0].measured ? 1U : 0U) |
                     (snapshot->task[1].measured ? 2U : 0U) |
                     (snapshot->task[2].measured ? 4U : 0U) |
                     (snapshot->task[3].measured ? 8U : 0U) |
                     (snapshot->task[4].measured ? 16U : 0U)),
      (unsigned long)snapshot->queue_current,
      (unsigned long)snapshot->queue_maximum,
      (unsigned long)snapshot->queue_depth,
      (unsigned long)snapshot->queue_dropped,
      (unsigned long)snapshot->queue_drained,
      (unsigned long)snapshot->health_state,
      (unsigned long)snapshot->health_warning_mask,
      (unsigned long)snapshot->health_stalled_mask,
      (unsigned long)snapshot->watchdog_feed,
      (unsigned long)snapshot->fault_code,
      (unsigned long)snapshot->reset_primary,
      (unsigned long)snapshot->reset_raw_flags,
      snapshot->reset_loop ? 1U : 0U,
      (unsigned long)snapshot->rs485_accepted,
      (unsigned long)snapshot->rs485_error_count,
      (unsigned long)snapshot->can_state,
      (unsigned long)snapshot->can_pending,
      (unsigned long)snapshot->can_capacity,
      (unsigned long)snapshot->can_maximum_pending,
      (unsigned long)snapshot->can_event_dropped,
      (unsigned long)snapshot->can_hal_busy,
      (unsigned long)snapshot->can_bus_off,
      (unsigned long)snapshot->can_recovery_attempts,
      (unsigned long)snapshot->sensor[0].state,
      (unsigned long)snapshot->sensor[0].sequence,
      (unsigned long)snapshot->sensor[0].fault_count,
      (unsigned long)snapshot->sensor[0].recovery_count,
      (unsigned long)snapshot->sensor[1].state,
      (unsigned long)snapshot->sensor[1].sequence,
      (unsigned long)snapshot->sensor[1].fault_count,
      (unsigned long)snapshot->sensor[1].recovery_count,
      (unsigned long)snapshot->sensor[2].state,
      (unsigned long)snapshot->sensor[2].sequence,
      (unsigned long)snapshot->sensor[2].fault_count,
      (unsigned long)snapshot->sensor[2].recovery_count,
      (unsigned long)snapshot->sensor[3].state,
      (unsigned long)snapshot->sensor[3].sequence,
      (unsigned long)snapshot->sensor[3].fault_count,
      (unsigned long)snapshot->sensor[3].recovery_count);

  if ((written < 0) || ((size_t)written >= capacity))
  {
    buffer[0] = '\0';
    *length = 0U;
    return false;
  }
  *length = (size_t)written;
  return true;
}
