#include "app_transport_policy.h"

#include <limits.h>
#include <string.h>

_Static_assert(sizeof(app_transport_event_t) == 12U,
               "transport event must remain a 12-byte by-value record");

static uint32_t app_transport_saturating_increment(uint32_t value)
{
  return value == UINT32_MAX ? UINT32_MAX : value + 1U;
}

static uint32_t app_transport_saturating_add(uint32_t value, size_t increment)
{
  if (increment >= (size_t)UINT32_MAX)
  {
    return UINT32_MAX;
  }

  const uint32_t bounded_increment = (uint32_t)increment;
  if (bounded_increment > (UINT32_MAX - value))
  {
    return UINT32_MAX;
  }
  return value + bounded_increment;
}

void app_transport_counters_initialize(app_transport_counters_t *counters)
{
  if (counters != NULL)
  {
    (void)memset(counters, 0, sizeof(*counters));
  }
}

bool app_transport_event_is_valid(const app_transport_event_t *event)
{
  return (event != NULL) && (event->source != 0U) && (event->code != 0U);
}

app_transport_event_result_t app_transport_event_admit(
    app_transport_counters_t *counters,
    const app_transport_event_t *event,
    bool queue_has_capacity)
{
  if ((counters == NULL) || !app_transport_event_is_valid(event))
  {
    if (counters != NULL)
    {
      counters->event_rejected_invalid_count =
          app_transport_saturating_increment(
              counters->event_rejected_invalid_count);
    }
    return APP_TRANSPORT_EVENT_REJECTED_INVALID;
  }

  if (!queue_has_capacity)
  {
    counters->event_dropped_full_count = app_transport_saturating_increment(
        counters->event_dropped_full_count);
    return APP_TRANSPORT_EVENT_DROPPED_FULL;
  }

  counters->event_accepted_count =
      app_transport_saturating_increment(counters->event_accepted_count);
  return APP_TRANSPORT_EVENT_ACCEPTED;
}

size_t app_transport_event_drain_count(size_t pending_count)
{
  return pending_count < APP_TRANSPORT_EVENT_DRAIN_BUDGET
             ? pending_count
             : APP_TRANSPORT_EVENT_DRAIN_BUDGET;
}

void app_transport_note_events_drained(app_transport_counters_t *counters,
                                       size_t drained_count)
{
  if (counters != NULL)
  {
    counters->event_drained_count = app_transport_saturating_add(
        counters->event_drained_count, drained_count);
  }
}

app_transport_command_result_t app_transport_command_admit(
    app_transport_counters_t *counters,
    bool command_is_valid,
    bool queue_has_capacity)
{
  if ((counters == NULL) || !command_is_valid)
  {
    if (counters != NULL)
    {
      counters->command_rejected_invalid_count =
          app_transport_saturating_increment(
              counters->command_rejected_invalid_count);
    }
    return APP_TRANSPORT_COMMAND_REJECTED_INVALID;
  }

  if (!queue_has_capacity)
  {
    counters->command_rejected_full_count =
        app_transport_saturating_increment(
            counters->command_rejected_full_count);
    return APP_TRANSPORT_COMMAND_REJECTED_FULL;
  }

  counters->command_accepted_count =
      app_transport_saturating_increment(counters->command_accepted_count);
  return APP_TRANSPORT_COMMAND_ACCEPTED;
}

void app_transport_note_snapshot_contention(
    app_transport_counters_t *counters,
    bool writer)
{
  if (counters == NULL)
  {
    return;
  }

  uint32_t *counter = writer ? &counters->snapshot_write_contention_count
                             : &counters->snapshot_read_contention_count;
  *counter = app_transport_saturating_increment(*counter);
}
