#ifndef APP_RESET_REASON_H
#define APP_RESET_REASON_H

#include <stdbool.h>
#include <stdint.h>

#define APP_RESET_RECORD_MAGIC UINT32_C(0x50355252)
#define APP_RESET_RECORD_VERSION (1U)
#define APP_RESET_LOOP_LIMIT (3U)

#define APP_RESET_REASON_POWER_ON (UINT32_C(1) << 0)
#define APP_RESET_REASON_BROWN_OUT (UINT32_C(1) << 1)
#define APP_RESET_REASON_PIN (UINT32_C(1) << 2)
#define APP_RESET_REASON_SOFTWARE (UINT32_C(1) << 3)
#define APP_RESET_REASON_IWDG (UINT32_C(1) << 4)
#define APP_RESET_REASON_WWDG (UINT32_C(1) << 5)
#define APP_RESET_REASON_LOW_POWER (UINT32_C(1) << 6)
#define APP_RESET_REASON_MULTIPLE (UINT32_C(1) << 7)
#define APP_RESET_REASON_UNKNOWN (UINT32_C(1) << 8)

typedef enum
{
  APP_RESET_PRIMARY_UNKNOWN = 0,
  APP_RESET_PRIMARY_POWER_ON,
  APP_RESET_PRIMARY_BROWN_OUT,
  APP_RESET_PRIMARY_PIN,
  APP_RESET_PRIMARY_SOFTWARE,
  APP_RESET_PRIMARY_IWDG,
  APP_RESET_PRIMARY_WWDG,
  APP_RESET_PRIMARY_LOW_POWER
} app_reset_primary_t;

typedef struct
{
  uint32_t hardware_raw_flags;
  uint32_t normalized_flags;
} app_reset_observation_t;

typedef struct
{
  uint32_t hardware_raw_flags;
  uint32_t reason_mask;
  app_reset_primary_t primary;
  bool multiple;
} app_reset_decoded_t;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size_bytes;
  uint32_t boot_count;
  uint32_t consecutive_recovery_resets;
  uint32_t last_reason_mask;
  uint32_t last_hardware_raw_flags;
  uint32_t last_fault_code;
  uint32_t checksum;
} app_reset_record_t;

app_reset_decoded_t app_reset_reason_decode(
    const app_reset_observation_t *observation);
void app_reset_record_initialize(app_reset_record_t *record);
bool app_reset_record_is_valid(const app_reset_record_t *record);
bool app_reset_record_note_boot(app_reset_record_t *record,
                                const app_reset_decoded_t *decoded,
                                uint32_t fault_code);
bool app_reset_record_note_fault(app_reset_record_t *record,
                                 uint32_t fault_code);
bool app_reset_record_note_stable(app_reset_record_t *record);
bool app_reset_record_loop_latched(const app_reset_record_t *record);

#endif
