#include "app_reset_reason.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint32_t app_reset_saturating_increment(uint32_t value)
{
  return value == UINT32_MAX ? UINT32_MAX : value + 1U;
}

static uint32_t app_reset_mix(uint32_t checksum, uint32_t value)
{
  checksum ^= value;
  checksum *= UINT32_C(16777619);
  return checksum;
}

static uint32_t app_reset_record_checksum(const app_reset_record_t *record)
{
  uint32_t checksum = UINT32_C(2166136261);
  checksum = app_reset_mix(checksum, record->magic);
  checksum = app_reset_mix(
      checksum,
      ((uint32_t)record->version << 16) | (uint32_t)record->size_bytes);
  checksum = app_reset_mix(checksum, record->boot_count);
  checksum = app_reset_mix(checksum, record->consecutive_recovery_resets);
  checksum = app_reset_mix(checksum, record->last_reason_mask);
  checksum = app_reset_mix(checksum, record->last_hardware_raw_flags);
  checksum = app_reset_mix(checksum, record->last_fault_code);
  return checksum;
}

static uint32_t app_reset_known_reason_mask(void)
{
  return APP_RESET_REASON_POWER_ON | APP_RESET_REASON_BROWN_OUT |
         APP_RESET_REASON_PIN | APP_RESET_REASON_SOFTWARE |
         APP_RESET_REASON_IWDG | APP_RESET_REASON_WWDG |
         APP_RESET_REASON_LOW_POWER;
}

static uint32_t app_reset_count_bits(uint32_t value)
{
  uint32_t count = 0U;
  while (value != 0U)
  {
    count += value & 1U;
    value >>= 1U;
  }
  return count;
}

static app_reset_primary_t app_reset_primary(uint32_t mask)
{
  if ((mask & APP_RESET_REASON_IWDG) != 0U)
  {
    return APP_RESET_PRIMARY_IWDG;
  }
  if ((mask & APP_RESET_REASON_WWDG) != 0U)
  {
    return APP_RESET_PRIMARY_WWDG;
  }
  if ((mask & APP_RESET_REASON_SOFTWARE) != 0U)
  {
    return APP_RESET_PRIMARY_SOFTWARE;
  }
  if ((mask & APP_RESET_REASON_BROWN_OUT) != 0U)
  {
    return APP_RESET_PRIMARY_BROWN_OUT;
  }
  if ((mask & APP_RESET_REASON_POWER_ON) != 0U)
  {
    return APP_RESET_PRIMARY_POWER_ON;
  }
  if ((mask & APP_RESET_REASON_PIN) != 0U)
  {
    return APP_RESET_PRIMARY_PIN;
  }
  if ((mask & APP_RESET_REASON_LOW_POWER) != 0U)
  {
    return APP_RESET_PRIMARY_LOW_POWER;
  }
  return APP_RESET_PRIMARY_UNKNOWN;
}

static bool app_reset_is_recovery_reset(uint32_t reason_mask)
{
  return (reason_mask & (APP_RESET_REASON_SOFTWARE | APP_RESET_REASON_IWDG |
                         APP_RESET_REASON_WWDG)) != 0U;
}

app_reset_decoded_t app_reset_reason_decode(
    const app_reset_observation_t *observation)
{
  app_reset_decoded_t decoded = {0U, APP_RESET_REASON_UNKNOWN,
                                 APP_RESET_PRIMARY_UNKNOWN, false};
  if (observation == NULL)
  {
    return decoded;
  }

  decoded.hardware_raw_flags = observation->hardware_raw_flags;
  decoded.reason_mask = observation->normalized_flags &
                        app_reset_known_reason_mask();
  if (decoded.reason_mask == 0U)
  {
    decoded.reason_mask = APP_RESET_REASON_UNKNOWN;
  }
  else
  {
    decoded.multiple = app_reset_count_bits(decoded.reason_mask) > 1U;
    if (decoded.multiple)
    {
      decoded.reason_mask |= APP_RESET_REASON_MULTIPLE;
    }
  }
  decoded.primary = app_reset_primary(decoded.reason_mask);
  return decoded;
}

void app_reset_record_initialize(app_reset_record_t *record)
{
  if (record == NULL)
  {
    return;
  }

  (void)memset(record, 0, sizeof(*record));
  record->magic = APP_RESET_RECORD_MAGIC;
  record->version = APP_RESET_RECORD_VERSION;
  record->size_bytes = (uint16_t)sizeof(*record);
  record->checksum = app_reset_record_checksum(record);
}

bool app_reset_record_is_valid(const app_reset_record_t *record)
{
  return (record != NULL) && (record->magic == APP_RESET_RECORD_MAGIC) &&
         (record->version == APP_RESET_RECORD_VERSION) &&
         (record->size_bytes == sizeof(*record)) &&
         (record->checksum == app_reset_record_checksum(record));
}

bool app_reset_record_note_boot(app_reset_record_t *record,
                                const app_reset_decoded_t *decoded,
                                uint32_t fault_code)
{
  if ((record == NULL) || (decoded == NULL))
  {
    return false;
  }
  if (!app_reset_record_is_valid(record))
  {
    app_reset_record_initialize(record);
  }

  record->boot_count = app_reset_saturating_increment(record->boot_count);
  if (app_reset_is_recovery_reset(decoded->reason_mask))
  {
    record->consecutive_recovery_resets = app_reset_saturating_increment(
        record->consecutive_recovery_resets);
  }
  else
  {
    record->consecutive_recovery_resets = 0U;
  }
  record->last_reason_mask = decoded->reason_mask;
  record->last_hardware_raw_flags = decoded->hardware_raw_flags;
  record->last_fault_code = fault_code;
  record->checksum = app_reset_record_checksum(record);
  return true;
}

bool app_reset_record_note_fault(app_reset_record_t *record,
                                 uint32_t fault_code)
{
  if (!app_reset_record_is_valid(record))
  {
    return false;
  }
  record->last_fault_code = fault_code;
  record->checksum = app_reset_record_checksum(record);
  return true;
}

bool app_reset_record_note_stable(app_reset_record_t *record)
{
  if (!app_reset_record_is_valid(record))
  {
    return false;
  }
  record->consecutive_recovery_resets = 0U;
  record->checksum = app_reset_record_checksum(record);
  return true;
}

bool app_reset_record_loop_latched(const app_reset_record_t *record)
{
  return app_reset_record_is_valid(record) &&
         (record->consecutive_recovery_resets >= APP_RESET_LOOP_LIMIT);
}
