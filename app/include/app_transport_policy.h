#ifndef APP_TRANSPORT_POLICY_H
#define APP_TRANSPORT_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_TRANSPORT_EVENT_QUEUE_DEPTH (8U)
#define APP_TRANSPORT_EVENT_DRAIN_BUDGET (2U)

typedef struct
{
  uint32_t timestamp_ms;
  uint32_t detail;
  uint16_t source;
  uint16_t code;
} app_transport_event_t;

typedef enum
{
  APP_TRANSPORT_EVENT_ACCEPTED = 0,
  APP_TRANSPORT_EVENT_DROPPED_FULL,
  APP_TRANSPORT_EVENT_REJECTED_INVALID
} app_transport_event_result_t;

typedef enum
{
  APP_TRANSPORT_COMMAND_ACCEPTED = 0,
  APP_TRANSPORT_COMMAND_REJECTED_FULL,
  APP_TRANSPORT_COMMAND_REJECTED_INVALID
} app_transport_command_result_t;

typedef struct
{
  uint32_t event_accepted_count;
  uint32_t event_dropped_full_count;
  uint32_t event_rejected_invalid_count;
  uint32_t event_drained_count;
  uint32_t command_accepted_count;
  uint32_t command_rejected_full_count;
  uint32_t command_rejected_invalid_count;
  uint32_t snapshot_read_contention_count;
  uint32_t snapshot_write_contention_count;
} app_transport_counters_t;

void app_transport_counters_initialize(app_transport_counters_t *counters);
bool app_transport_event_is_valid(const app_transport_event_t *event);
app_transport_event_result_t app_transport_event_admit(
    app_transport_counters_t *counters,
    const app_transport_event_t *event,
    bool queue_has_capacity);
size_t app_transport_event_drain_count(size_t pending_count);
void app_transport_note_events_drained(app_transport_counters_t *counters,
                                       size_t drained_count);
app_transport_command_result_t app_transport_command_admit(
    app_transport_counters_t *counters,
    bool command_is_valid,
    bool queue_has_capacity);
void app_transport_note_snapshot_contention(
    app_transport_counters_t *counters,
    bool writer);

#endif
