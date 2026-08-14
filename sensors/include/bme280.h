#ifndef BME280_H
#define BME280_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BME280_CHIP_ID_REGISTER UINT8_C(0xd0)
#define BME280_CHIP_ID UINT8_C(0x60)
#define BME280_BMP280_CHIP_ID UINT8_C(0x58)
#define BME280_RESET_REGISTER UINT8_C(0xe0)
#define BME280_RESET_COMMAND UINT8_C(0xb6)
#define BME280_STATUS_REGISTER UINT8_C(0xf3)
#define BME280_CTRL_HUM_REGISTER UINT8_C(0xf2)
#define BME280_CTRL_MEAS_REGISTER UINT8_C(0xf4)
#define BME280_CONFIG_REGISTER UINT8_C(0xf5)
#define BME280_CALIBRATION_1_REGISTER UINT8_C(0x88)
#define BME280_CALIBRATION_2_REGISTER UINT8_C(0xe1)
#define BME280_RAW_DATA_REGISTER UINT8_C(0xf7)

#define BME280_CALIBRATION_1_LENGTH (26U)
#define BME280_CALIBRATION_2_LENGTH (7U)
#define BME280_RAW_DATA_LENGTH (8U)

#define BME280_STATUS_MEASURING_MASK UINT8_C(0x08)
#define BME280_STATUS_IM_UPDATE_MASK UINT8_C(0x01)
#define BME280_CTRL_HUMIDITY_X1 UINT8_C(0x01)
#define BME280_CONFIG_FILTER_OFF UINT8_C(0x00)
#define BME280_CTRL_MEAS_X1_X1_FORCED UINT8_C(0x25)

#define BME280_SAMPLE_VALID_TEMPERATURE (UINT32_C(1) << 0)
#define BME280_SAMPLE_VALID_PRESSURE (UINT32_C(1) << 1)
#define BME280_SAMPLE_VALID_HUMIDITY (UINT32_C(1) << 2)
#define BME280_SAMPLE_VALID_ALL                                          \
  (BME280_SAMPLE_VALID_TEMPERATURE | BME280_SAMPLE_VALID_PRESSURE |      \
   BME280_SAMPLE_VALID_HUMIDITY)

#define BME280_DEFAULT_SAMPLE_PERIOD_MS UINT32_C(1000)
#define BME280_RESET_READY_DELAY_MS UINT32_C(20)
#define BME280_RESET_DEADLINE_MS UINT32_C(100)
#define BME280_MEASUREMENT_READY_DELAY_MS UINT32_C(20)
#define BME280_MEASUREMENT_DEADLINE_MS UINT32_C(40)

typedef enum
{
  BME280_TRANSPORT_OK = 0,
  BME280_TRANSPORT_INVALID_ARGUMENT,
  BME280_TRANSPORT_BUSY,
  BME280_TRANSPORT_TIMEOUT,
  BME280_TRANSPORT_IO_ERROR
} bme280_transport_result_t;

typedef enum
{
  BME280_STATUS_UNINITIALIZED = 0,
  BME280_STATUS_INITIALIZING,
  BME280_STATUS_VALID,
  BME280_STATUS_NOT_READY,
  BME280_STATUS_WRONG_ID,
  BME280_STATUS_UNSUPPORTED_BMP280,
  BME280_STATUS_CALIBRATION_INVALID,
  BME280_STATUS_TRANSPORT_INVALID_ARGUMENT,
  BME280_STATUS_TRANSPORT_BUSY,
  BME280_STATUS_TRANSPORT_TIMEOUT,
  BME280_STATUS_TRANSPORT_IO_ERROR,
  BME280_STATUS_MEASUREMENT_INVALID,
  BME280_STATUS_NVM_TIMEOUT,
  BME280_STATUS_MEASUREMENT_TIMEOUT,
  BME280_STATUS_RECOVERY_REQUIRED,
  BME280_STATUS_OFFLINE
} bme280_status_t;

typedef enum
{
  BME280_STATE_UNINITIALIZED = 0,
  BME280_STATE_READ_ID,
  BME280_STATE_SOFT_RESET,
  BME280_STATE_WAIT_NVM,
  BME280_STATE_READ_CALIBRATION_1,
  BME280_STATE_READ_CALIBRATION_2,
  BME280_STATE_CONFIGURE_HUMIDITY,
  BME280_STATE_CONFIGURE_FILTER,
  BME280_STATE_IDLE,
  BME280_STATE_START_FORCED,
  BME280_STATE_WAIT_MEASUREMENT,
  BME280_STATE_READ_RAW,
  BME280_STATE_OFFLINE
} bme280_state_t;

typedef struct
{
  uint16_t dig_t1;
  int16_t dig_t2;
  int16_t dig_t3;
  uint16_t dig_p1;
  int16_t dig_p2;
  int16_t dig_p3;
  int16_t dig_p4;
  int16_t dig_p5;
  int16_t dig_p6;
  int16_t dig_p7;
  int16_t dig_p8;
  int16_t dig_p9;
  uint8_t dig_h1;
  int16_t dig_h2;
  uint8_t dig_h3;
  int16_t dig_h4;
  int16_t dig_h5;
  int8_t dig_h6;
} bme280_calibration_t;

typedef struct
{
  uint32_t temperature;
  uint32_t pressure;
  uint16_t humidity;
} bme280_raw_sample_t;

typedef struct
{
  bme280_status_t status;
  uint32_t valid_mask;
  uint32_t sequence;
  uint32_t raw_temperature;
  uint32_t raw_pressure;
  uint16_t raw_humidity;
  int32_t temperature_centi_c;
  uint32_t pressure_pa;
  uint32_t humidity_milli_pct;
} bme280_sample_t;

typedef struct
{
  void *context;
  bme280_transport_result_t (*read)(void *context,
                                    uint8_t register_address,
                                    uint8_t *data,
                                    size_t length);
  bme280_transport_result_t (*write)(void *context,
                                     uint8_t register_address,
                                     uint8_t value);
} bme280_ops_t;

typedef struct
{
  bme280_state_t state;
  bme280_status_t status;
  bme280_transport_result_t last_transport_result;
  bme280_ops_t ops;
  bme280_calibration_t calibration;
  bme280_sample_t sample;
  uint8_t calibration_1[BME280_CALIBRATION_1_LENGTH];
  uint8_t calibration_2[BME280_CALIBRATION_2_LENGTH];
  uint32_t next_action_ms;
  uint32_t deadline_ms;
  uint32_t sample_period_ms;
  uint32_t transaction_count;
  uint32_t error_count;
  uint32_t recovery_request_count;
  uint32_t recovery_success_count;
  uint8_t recovery_attempts;
  bool recovery_active;
} bme280_t;

bool bme280_parse_calibration(
    const uint8_t calibration_1[BME280_CALIBRATION_1_LENGTH],
    const uint8_t calibration_2[BME280_CALIBRATION_2_LENGTH],
    bme280_calibration_t *calibration);
bool bme280_calibration_is_valid(
    const uint8_t calibration_1[BME280_CALIBRATION_1_LENGTH],
    const uint8_t calibration_2[BME280_CALIBRATION_2_LENGTH],
    const bme280_calibration_t *calibration);
bool bme280_parse_raw(const uint8_t data[BME280_RAW_DATA_LENGTH],
                      bme280_raw_sample_t *raw);
bool bme280_compensate(const bme280_calibration_t *calibration,
                       const bme280_raw_sample_t *raw,
                       bme280_sample_t *sample);
bool bme280_initialize(bme280_t *driver,
                       const bme280_ops_t *ops,
                       uint32_t sample_period_ms);
bool bme280_service(bme280_t *driver, uint32_t now_ms);
bool bme280_request_reinitialize(bme280_t *driver);
bool bme280_get_sample(const bme280_t *driver, bme280_sample_t *sample);

#endif
