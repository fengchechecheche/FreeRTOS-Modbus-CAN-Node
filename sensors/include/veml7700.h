#ifndef VEML7700_H
#define VEML7700_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VEML7700_ADDRESS_7BIT UINT8_C(0x10)
#define VEML7700_CONFIG_REGISTER UINT8_C(0x00)
#define VEML7700_ALS_REGISTER UINT8_C(0x04)
#define VEML7700_WORD_LENGTH (2U)

#define VEML7700_DEFAULT_RANGE_LEVEL (2U)
#define VEML7700_RANGE_LEVEL_COUNT (9U)
#define VEML7700_LOW_COUNT_THRESHOLD UINT16_C(100)
#define VEML7700_HIGH_COUNT_THRESHOLD UINT16_C(10000)
#define VEML7700_SATURATION_THRESHOLD UINT16_C(0xff00)
#define VEML7700_DEFAULT_SAMPLE_PERIOD_MS UINT32_C(1000)
#define VEML7700_HIGH_LUX_UNCORRECTED_MILLILUX UINT32_C(1000000)

#define VEML7700_QUALITY_VALID (UINT32_C(1) << 0)
#define VEML7700_QUALITY_RANGING (UINT32_C(1) << 1)
#define VEML7700_QUALITY_LOW_COUNT (UINT32_C(1) << 2)
#define VEML7700_QUALITY_SATURATED (UINT32_C(1) << 3)
#define VEML7700_QUALITY_RANGE_LIMITED (UINT32_C(1) << 4)
#define VEML7700_QUALITY_HIGH_LUX_UNCORRECTED (UINT32_C(1) << 5)

typedef enum
{
  VEML7700_TRANSPORT_OK = 0,
  VEML7700_TRANSPORT_INVALID_ARGUMENT,
  VEML7700_TRANSPORT_NOT_PRESENT,
  VEML7700_TRANSPORT_BUSY,
  VEML7700_TRANSPORT_TIMEOUT,
  VEML7700_TRANSPORT_IO_ERROR
} veml7700_transport_result_t;

typedef enum
{
  VEML7700_STATUS_UNINITIALIZED = 0,
  VEML7700_STATUS_INITIALIZING,
  VEML7700_STATUS_RANGING,
  VEML7700_STATUS_VALID,
  VEML7700_STATUS_NOT_PRESENT,
  VEML7700_STATUS_TRANSPORT_INVALID_ARGUMENT,
  VEML7700_STATUS_TRANSPORT_BUSY,
  VEML7700_STATUS_TRANSPORT_TIMEOUT,
  VEML7700_STATUS_TRANSPORT_IO_ERROR,
  VEML7700_STATUS_CONFIGURATION_MISMATCH,
  VEML7700_STATUS_SATURATED,
  VEML7700_STATUS_RANGE_LIMITED,
  VEML7700_STATUS_RECOVERY_REQUIRED,
  VEML7700_STATUS_RECOVERY_UNAVAILABLE,
  VEML7700_STATUS_OFFLINE
} veml7700_status_t;

typedef enum
{
  VEML7700_STATE_UNINITIALIZED = 0,
  VEML7700_STATE_READ_CONFIG,
  VEML7700_STATE_WRITE_CONFIG,
  VEML7700_STATE_VERIFY_CONFIG,
  VEML7700_STATE_WAIT_INTEGRATION,
  VEML7700_STATE_READ_ALS,
  VEML7700_STATE_IDLE,
  VEML7700_STATE_OFFLINE
} veml7700_state_t;

typedef enum
{
  VEML7700_GAIN_ONE_EIGHTH = 0,
  VEML7700_GAIN_ONE_QUARTER,
  VEML7700_GAIN_ONE,
  VEML7700_GAIN_TWO
} veml7700_gain_t;

typedef struct
{
  uint16_t config_word;
  uint16_t integration_ms;
  uint16_t resolution_0p0001_lux;
  veml7700_gain_t gain;
} veml7700_range_config_t;

typedef struct
{
  veml7700_status_t status;
  uint32_t quality_flags;
  uint32_t sequence;
  uint32_t illuminance_millilux;
  uint16_t raw_als;
  uint16_t integration_ms;
  uint8_t range_level;
  veml7700_gain_t gain;
} veml7700_sample_t;

typedef struct
{
  void *context;
  veml7700_transport_result_t (*read)(void *context,
                                      uint8_t register_address,
                                      uint8_t *data,
                                      size_t length);
  veml7700_transport_result_t (*write)(void *context,
                                       uint8_t register_address,
                                       const uint8_t *data,
                                       size_t length);
  bool (*request_recovery)(void *context);
} veml7700_ops_t;

typedef struct
{
  veml7700_state_t state;
  veml7700_status_t status;
  veml7700_status_t last_error_status;
  veml7700_transport_result_t last_transport_result;
  veml7700_ops_t ops;
  veml7700_sample_t sample;
  uint32_t next_action_ms;
  uint32_t sample_period_ms;
  uint32_t transaction_count;
  uint32_t error_count;
  uint32_t range_change_count;
  uint32_t recovery_request_count;
  uint32_t recovery_success_count;
  uint8_t range_level;
  uint8_t ranging_adjustments;
  uint8_t recovery_attempts;
  bool recovery_active;
} veml7700_t;

bool veml7700_get_range_config(uint8_t range_level,
                               veml7700_range_config_t *config);
uint16_t veml7700_decode_word(
    const uint8_t data[VEML7700_WORD_LENGTH]);
bool veml7700_encode_word(uint16_t value,
                          uint8_t data[VEML7700_WORD_LENGTH]);
bool veml7700_convert_millilux(uint16_t raw_als,
                               uint8_t range_level,
                               uint32_t *illuminance_millilux);
bool veml7700_initialize(veml7700_t *driver,
                         const veml7700_ops_t *ops,
                         uint32_t sample_period_ms);
bool veml7700_service(veml7700_t *driver, uint32_t now_ms);
bool veml7700_request_reinitialize(veml7700_t *driver);
bool veml7700_get_sample(const veml7700_t *driver,
                         veml7700_sample_t *sample);

#endif
