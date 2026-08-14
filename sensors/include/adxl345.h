#ifndef ADXL345_H
#define ADXL345_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ADXL345_DEVID_REGISTER UINT8_C(0x00)
#define ADXL345_DEVID_VALUE UINT8_C(0xe5)
#define ADXL345_BW_RATE_REGISTER UINT8_C(0x2c)
#define ADXL345_POWER_CTL_REGISTER UINT8_C(0x2d)
#define ADXL345_INT_ENABLE_REGISTER UINT8_C(0x2e)
#define ADXL345_INT_MAP_REGISTER UINT8_C(0x2f)
#define ADXL345_DATA_FORMAT_REGISTER UINT8_C(0x31)
#define ADXL345_DATAX0_REGISTER UINT8_C(0x32)
#define ADXL345_FIFO_CTL_REGISTER UINT8_C(0x38)

#define ADXL345_BW_RATE_100_HZ UINT8_C(0x0a)
#define ADXL345_POWER_CTL_STANDBY UINT8_C(0x00)
#define ADXL345_POWER_CTL_MEASURE UINT8_C(0x08)
#define ADXL345_INT_DATA_READY UINT8_C(0x80)
#define ADXL345_INT_MAP_DATA_READY_INT1 UINT8_C(0x00)
#define ADXL345_DATA_FORMAT_FULL_RES_4G UINT8_C(0x09)
#define ADXL345_FIFO_BYPASS UINT8_C(0x00)

#define ADXL345_DATA_LENGTH (6U)
#define ADXL345_AXIS_COUNT (3U)
#define ADXL345_FEATURE_WINDOW_SAMPLES (100U)
#define ADXL345_DATA_READY_STALL_MS UINT32_C(100)

#define ADXL345_QUALITY_VALID (UINT32_C(1) << 0)
#define ADXL345_QUALITY_BIAS_UNCALIBRATED (UINT32_C(1) << 1)
#define ADXL345_QUALITY_GAP (UINT32_C(1) << 2)
#define ADXL345_QUALITY_DROPPED (UINT32_C(1) << 3)
#define ADXL345_QUALITY_TRANSPORT_ERROR (UINT32_C(1) << 4)
#define ADXL345_QUALITY_CONFIGURATION_ERROR (UINT32_C(1) << 5)
#define ADXL345_QUALITY_STALLED (UINT32_C(1) << 6)

typedef enum
{
  ADXL345_TRANSPORT_OK = 0,
  ADXL345_TRANSPORT_INVALID_ARGUMENT,
  ADXL345_TRANSPORT_BUSY,
  ADXL345_TRANSPORT_TIMEOUT,
  ADXL345_TRANSPORT_IO_ERROR
} adxl345_transport_result_t;

typedef enum
{
  ADXL345_STATUS_UNINITIALIZED = 0,
  ADXL345_STATUS_INITIALIZING,
  ADXL345_STATUS_VALID,
  ADXL345_STATUS_WRONG_ID,
  ADXL345_STATUS_TRANSPORT_INVALID_ARGUMENT,
  ADXL345_STATUS_TRANSPORT_BUSY,
  ADXL345_STATUS_TRANSPORT_TIMEOUT,
  ADXL345_STATUS_TRANSPORT_IO_ERROR,
  ADXL345_STATUS_CONFIGURATION_MISMATCH,
  ADXL345_STATUS_DATA_READY_STALLED,
  ADXL345_STATUS_RECOVERY_REQUIRED,
  ADXL345_STATUS_OFFLINE
} adxl345_status_t;

typedef enum
{
  ADXL345_STATE_UNINITIALIZED = 0,
  ADXL345_STATE_READ_ID,
  ADXL345_STATE_WRITE_STANDBY,
  ADXL345_STATE_DISABLE_INTERRUPTS,
  ADXL345_STATE_SET_FIFO_BYPASS,
  ADXL345_STATE_SET_DATA_FORMAT,
  ADXL345_STATE_SET_BW_RATE,
  ADXL345_STATE_MAP_INT1,
  ADXL345_STATE_VERIFY_DATA_FORMAT,
  ADXL345_STATE_VERIFY_BW_RATE,
  ADXL345_STATE_VERIFY_INT_MAP,
  ADXL345_STATE_ENTER_MEASURE,
  ADXL345_STATE_VERIFY_POWER_CTL,
  ADXL345_STATE_ENABLE_DATA_READY,
  ADXL345_STATE_WAIT_DATA_READY,
  ADXL345_STATE_OFFLINE
} adxl345_state_t;

typedef struct
{
  int16_t bias_lsb[ADXL345_AXIS_COUNT];
  bool bias_calibrated;
} adxl345_config_t;

typedef struct
{
  adxl345_status_t status;
  int16_t raw_xyz[ADXL345_AXIS_COUNT];
  int32_t corrected_counts[ADXL345_AXIS_COUNT];
  int32_t acceleration_millig[ADXL345_AXIS_COUNT];
  uint32_t sequence;
  uint32_t quality_flags;
  uint32_t dropped_sample_lower_bound;
} adxl345_sample_t;

typedef struct
{
  uint32_t sequence;
  uint32_t sample_count;
  int32_t mean_millig[ADXL345_AXIS_COUNT];
  uint32_t rms_millig[ADXL345_AXIS_COUNT];
  uint32_t peak_abs_millig[ADXL345_AXIS_COUNT];
  uint32_t resultant_rms_millig;
  uint32_t quality_flags;
  uint32_t dropped_sample_lower_bound;
} adxl345_feature_t;

typedef struct
{
  uint32_t count;
  int64_t sum_millig[ADXL345_AXIS_COUNT];
  uint64_t sum_square_millig[ADXL345_AXIS_COUNT];
  int32_t minimum_millig[ADXL345_AXIS_COUNT];
  int32_t maximum_millig[ADXL345_AXIS_COUNT];
  uint32_t quality_flags;
  uint32_t dropped_sample_lower_bound;
} adxl345_feature_window_t;

typedef struct
{
  void *context;
  adxl345_transport_result_t (*read)(void *context,
                                     uint8_t register_address,
                                     uint8_t *data,
                                     size_t length);
  adxl345_transport_result_t (*write)(void *context,
                                      uint8_t register_address,
                                      uint8_t value);
} adxl345_ops_t;

typedef struct
{
  adxl345_state_t state;
  adxl345_status_t status;
  adxl345_status_t last_error_status;
  adxl345_transport_result_t last_transport_result;
  adxl345_ops_t ops;
  adxl345_config_t config;
  adxl345_sample_t sample;
  adxl345_feature_t feature;
  adxl345_feature_window_t window;
  uint32_t data_ready_deadline_ms;
  uint32_t transaction_count;
  uint32_t error_count;
  uint32_t recovery_request_count;
  uint32_t recovery_success_count;
  uint32_t irq_event_count;
  uint32_t dropped_sample_lower_bound;
  uint32_t pending_dropped_sample_lower_bound;
  uint8_t recovery_attempts;
  bool recovery_active;
  bool pending_gap;
} adxl345_t;

bool adxl345_parse_xyz(const uint8_t data[ADXL345_DATA_LENGTH],
                       int16_t raw_xyz[ADXL345_AXIS_COUNT]);
int32_t adxl345_counts_to_millig(int32_t corrected_counts);
void adxl345_feature_window_initialize(adxl345_feature_window_t *window);
bool adxl345_feature_window_push(adxl345_feature_window_t *window,
                                 const adxl345_sample_t *sample,
                                 adxl345_feature_t *feature);
bool adxl345_initialize(adxl345_t *driver,
                        const adxl345_ops_t *ops,
                        const adxl345_config_t *config);
bool adxl345_service(adxl345_t *driver,
                     uint32_t now_ms,
                     uint32_t data_ready_event_count);
bool adxl345_request_reinitialize(adxl345_t *driver);
bool adxl345_get_sample(const adxl345_t *driver, adxl345_sample_t *sample);
bool adxl345_get_feature(const adxl345_t *driver,
                         adxl345_feature_t *feature);

#endif
