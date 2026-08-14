#include "app_transport_policy.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define CHECK(condition)          \
  do                              \
  {                               \
    if (!(condition))             \
    {                             \
      return EXIT_FAILURE;        \
    }                             \
  } while (false)

typedef struct
{
  app_transport_event_t items[APP_TRANSPORT_EVENT_QUEUE_DEPTH];
  size_t read_index;
  size_t write_index;
  size_t count;
} fake_event_queue_t;

static bool fake_publish(fake_event_queue_t *queue,
                         app_transport_counters_t *counters,
                         const app_transport_event_t *event)
{
  const app_transport_event_result_t result = app_transport_event_admit(
      counters, event, queue->count < APP_TRANSPORT_EVENT_QUEUE_DEPTH);
  if (result != APP_TRANSPORT_EVENT_ACCEPTED)
  {
    return false;
  }

  queue->items[queue->write_index] = *event;
  queue->write_index =
      (queue->write_index + 1U) % APP_TRANSPORT_EVENT_QUEUE_DEPTH;
  ++queue->count;
  return true;
}

static size_t fake_drain(fake_event_queue_t *queue,
                         app_transport_counters_t *counters,
                         app_transport_event_t *output)
{
  const size_t drain_count = app_transport_event_drain_count(queue->count);
  for (size_t index = 0U; index < drain_count; ++index)
  {
    output[index] = queue->items[queue->read_index];
    queue->read_index =
        (queue->read_index + 1U) % APP_TRANSPORT_EVENT_QUEUE_DEPTH;
    --queue->count;
  }
  app_transport_note_events_drained(counters, drain_count);
  return drain_count;
}

static int test_burst_drop_new_and_fifo(void)
{
  fake_event_queue_t queue = {0};
  app_transport_counters_t counters;
  app_transport_counters_initialize(&counters);

  for (uint32_t index = 0U; index < 12U; ++index)
  {
    app_transport_event_t event = {
        index,
        index + 100U,
        1U,
        (uint16_t)(index + 1U),
    };
    const bool accepted = fake_publish(&queue, &counters, &event);
    CHECK(accepted == (index < APP_TRANSPORT_EVENT_QUEUE_DEPTH));
    event.detail = 0U;
  }

  CHECK(queue.count == APP_TRANSPORT_EVENT_QUEUE_DEPTH);
  CHECK(counters.event_accepted_count == 8U);
  CHECK(counters.event_dropped_full_count == 4U);

  app_transport_event_t drained[APP_TRANSPORT_EVENT_DRAIN_BUDGET];
  CHECK(fake_drain(&queue, &counters, drained) == 2U);
  CHECK(drained[0].code == 1U);
  CHECK(drained[0].detail == 100U);
  CHECK(drained[1].code == 2U);
  CHECK(queue.count == 6U);
  CHECK(counters.event_drained_count == 2U);
  return EXIT_SUCCESS;
}

static int test_slow_consumer_remains_bounded(void)
{
  fake_event_queue_t queue = {0};
  app_transport_counters_t counters;
  app_transport_counters_initialize(&counters);
  app_transport_event_t drained[APP_TRANSPORT_EVENT_DRAIN_BUDGET];

  for (uint32_t round = 0U; round < 4U; ++round)
  {
    for (uint32_t item = 0U; item < 4U; ++item)
    {
      const app_transport_event_t event = {
          round,
          item,
          1U,
          (uint16_t)(item + 1U),
      };
      (void)fake_publish(&queue, &counters, &event);
    }
    CHECK(fake_drain(&queue, &counters, drained) <=
          APP_TRANSPORT_EVENT_DRAIN_BUDGET);
  }

  CHECK(counters.event_accepted_count + counters.event_dropped_full_count ==
        16U);
  CHECK(counters.event_drained_count == 8U);
  CHECK(queue.count <= APP_TRANSPORT_EVENT_QUEUE_DEPTH);
  return EXIT_SUCCESS;
}

static int test_invalid_wrap_and_counter_saturation(void)
{
  app_transport_counters_t counters;
  app_transport_counters_initialize(&counters);
  CHECK(app_transport_event_admit(&counters, NULL, true) ==
        APP_TRANSPORT_EVENT_REJECTED_INVALID);

  const app_transport_event_t invalid = {0U, 0U, 0U, 1U};
  CHECK(app_transport_event_admit(&counters, &invalid, true) ==
        APP_TRANSPORT_EVENT_REJECTED_INVALID);

  const app_transport_event_t wrapped = {
      UINT32_C(0xffffffff), UINT32_C(0x12345678), 1U, 1U};
  CHECK(app_transport_event_admit(&counters, &wrapped, true) ==
        APP_TRANSPORT_EVENT_ACCEPTED);

  counters.event_dropped_full_count = UINT32_MAX;
  CHECK(app_transport_event_admit(&counters, &wrapped, false) ==
        APP_TRANSPORT_EVENT_DROPPED_FULL);
  CHECK(counters.event_dropped_full_count == UINT32_MAX);
  counters.event_drained_count = UINT32_MAX - 1U;
  app_transport_note_events_drained(&counters, 2U);
  CHECK(counters.event_drained_count == UINT32_MAX);
  return EXIT_SUCCESS;
}

static int test_command_and_snapshot_policies(void)
{
  app_transport_counters_t counters;
  app_transport_counters_initialize(&counters);

  CHECK(app_transport_command_admit(&counters, true, true) ==
        APP_TRANSPORT_COMMAND_ACCEPTED);
  CHECK(app_transport_command_admit(&counters, true, false) ==
        APP_TRANSPORT_COMMAND_REJECTED_FULL);
  CHECK(app_transport_command_admit(&counters, false, true) ==
        APP_TRANSPORT_COMMAND_REJECTED_INVALID);
  CHECK(counters.command_accepted_count == 1U);
  CHECK(counters.command_rejected_full_count == 1U);
  CHECK(counters.command_rejected_invalid_count == 1U);

  app_transport_note_snapshot_contention(&counters, false);
  app_transport_note_snapshot_contention(&counters, true);
  CHECK(counters.snapshot_read_contention_count == 1U);
  CHECK(counters.snapshot_write_contention_count == 1U);
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(sizeof(app_transport_event_t) == 12U);
  CHECK(test_burst_drop_new_and_fifo() == EXIT_SUCCESS);
  CHECK(test_slow_consumer_remains_bounded() == EXIT_SUCCESS);
  CHECK(test_invalid_wrap_and_counter_saturation() == EXIT_SUCCESS);
  CHECK(test_command_and_snapshot_policies() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}
