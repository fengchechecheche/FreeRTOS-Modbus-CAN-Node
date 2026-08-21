#include "app_soak_diagnostic.h"

#include <string.h>

typedef struct
{
  char *buffer;
  size_t capacity;
  size_t length;
  bool valid;
} app_soak_writer_t;

static void app_soak_append_char(app_soak_writer_t *writer, char value)
{
  if (!writer->valid || ((writer->length + 1U) >= writer->capacity))
  {
    writer->valid = false;
    return;
  }
  writer->buffer[writer->length++] = value;
}

static void app_soak_append_literal(app_soak_writer_t *writer,
                                    const char *value)
{
  const size_t value_length = strlen(value);
  if (!writer->valid || (value_length >= (writer->capacity - writer->length)))
  {
    writer->valid = false;
    return;
  }
  memcpy(&writer->buffer[writer->length], value, value_length);
  writer->length += value_length;
}

static void app_soak_append_u32(app_soak_writer_t *writer, uint32_t value)
{
  char digits[10];
  size_t count = 0U;
  do
  {
    digits[count++] = (char)('0' + (value % UINT32_C(10)));
    value /= UINT32_C(10);
  } while (value != 0U);

  while (count > 0U)
  {
    app_soak_append_char(writer, digits[--count]);
  }
}

static void app_soak_append_hex(app_soak_writer_t *writer,
                                uint32_t value,
                                size_t width)
{
  static const char digits[] = "0123456789ABCDEF";
  while (width > 0U)
  {
    const size_t shift = (--width) * 4U;
    app_soak_append_char(writer, digits[(value >> shift) & UINT32_C(0x0F)]);
  }
}

static void app_soak_append_task_field(app_soak_writer_t *writer,
                                       const char *prefix,
                                       const app_soak_diagnostic_snapshot_t *snapshot,
                                       size_t field)
{
  app_soak_append_literal(writer, prefix);
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    const uint32_t values[] = {
        snapshot->task[index].release,
        snapshot->task[index].missed,
        snapshot->task[index].deadline_miss,
        snapshot->task[index].budget_overrun,
        snapshot->task[index].configured_words,
        snapshot->task[index].minimum_free_words,
    };
    if (index > 0U)
    {
      app_soak_append_char(writer, '/');
    }
    app_soak_append_u32(writer, values[field]);
  }
}

static void app_soak_append_sensor(app_soak_writer_t *writer,
                                   const app_soak_sensor_t *sensor)
{
  app_soak_append_u32(writer, sensor->state);
  app_soak_append_char(writer, ',');
  app_soak_append_u32(writer, sensor->sequence);
  app_soak_append_char(writer, ',');
  app_soak_append_u32(writer, sensor->fault_count);
  app_soak_append_char(writer, ',');
  app_soak_append_u32(writer, sensor->recovery_count);
}

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

  app_soak_writer_t writer = {
      .buffer = buffer,
      .capacity = capacity,
      .length = 0U,
      .valid = true,
  };
  uint32_t measured_mask = 0U;
  for (size_t index = 0U; index < APP_TASK_COUNT; ++index)
  {
    if (snapshot->task[index].measured)
    {
      measured_mask |= UINT32_C(1) << index;
    }
  }

  app_soak_append_literal(&writer, "P5DIAG1 v=1 t=");
  app_soak_append_u32(&writer, snapshot->now_ms);
  app_soak_append_literal(&writer, " boot=");
  app_soak_append_u32(&writer, snapshot->boot_count);
  app_soak_append_task_field(&writer, " tr=", snapshot, 0U);
  app_soak_append_task_field(&writer, " tm=", snapshot, 1U);
  app_soak_append_task_field(&writer, " td=", snapshot, 2U);
  app_soak_append_task_field(&writer, " tb=", snapshot, 3U);
  app_soak_append_task_field(&writer, " tc=", snapshot, 4U);
  app_soak_append_task_field(&writer, " ts=", snapshot, 5U);
  app_soak_append_literal(&writer, " sm=");
  app_soak_append_hex(&writer, measured_mask, 2U);

  app_soak_append_literal(&writer, " q=");
  app_soak_append_u32(&writer, snapshot->queue_current);
  app_soak_append_char(&writer, '/');
  app_soak_append_u32(&writer, snapshot->queue_maximum);
  app_soak_append_char(&writer, '/');
  app_soak_append_u32(&writer, snapshot->queue_depth);
  app_soak_append_char(&writer, '/');
  app_soak_append_u32(&writer, snapshot->queue_dropped);
  app_soak_append_char(&writer, '/');
  app_soak_append_u32(&writer, snapshot->queue_drained);

  app_soak_append_literal(&writer, " h=");
  app_soak_append_u32(&writer, snapshot->health_state);
  app_soak_append_char(&writer, '/');
  app_soak_append_hex(&writer, snapshot->health_warning_mask, 8U);
  app_soak_append_char(&writer, '/');
  app_soak_append_hex(&writer, snapshot->health_stalled_mask, 8U);
  app_soak_append_char(&writer, '/');
  app_soak_append_u32(&writer, snapshot->watchdog_feed);
  app_soak_append_char(&writer, '/');
  app_soak_append_u32(&writer, snapshot->fault_code);

  app_soak_append_literal(&writer, " rst=");
  app_soak_append_u32(&writer, snapshot->reset_primary);
  app_soak_append_char(&writer, '/');
  app_soak_append_hex(&writer, snapshot->reset_raw_flags, 8U);
  app_soak_append_char(&writer, '/');
  app_soak_append_u32(&writer, snapshot->reset_loop ? 1U : 0U);

  app_soak_append_literal(&writer, " rs=");
  app_soak_append_u32(&writer, snapshot->rs485_accepted);
  app_soak_append_char(&writer, '/');
  app_soak_append_u32(&writer, snapshot->rs485_error_count);

  app_soak_append_literal(&writer, " can=");
  const uint32_t can_values[] = {
      snapshot->can_state,
      snapshot->can_pending,
      snapshot->can_capacity,
      snapshot->can_maximum_pending,
      snapshot->can_event_dropped,
      snapshot->can_hal_busy,
      snapshot->can_bus_off,
      snapshot->can_recovery_attempts,
  };
  for (size_t index = 0U; index < (sizeof(can_values) / sizeof(can_values[0]));
       ++index)
  {
    if (index > 0U)
    {
      app_soak_append_char(&writer, '/');
    }
    app_soak_append_u32(&writer, can_values[index]);
  }

  app_soak_append_literal(&writer, " sns=");
  for (size_t index = 0U; index < APP_SOAK_DIAGNOSTIC_SOURCE_COUNT; ++index)
  {
    if (index > 0U)
    {
      app_soak_append_char(&writer, ';');
    }
    app_soak_append_sensor(&writer, &snapshot->sensor[index]);
  }
  app_soak_append_literal(&writer, "\r\n");

  if (!writer.valid)
  {
    buffer[0] = '\0';
    *length = 0U;
    return false;
  }
  buffer[writer.length] = '\0';
  *length = writer.length;
  return true;
}
