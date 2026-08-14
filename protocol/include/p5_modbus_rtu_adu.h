#ifndef P5_MODBUS_RTU_ADU_H
#define P5_MODBUS_RTU_ADU_H

#include <stddef.h>
#include <stdint.h>

#define P5_MODBUS_RTU_MIN_ADU_SIZE (4U)
#define P5_MODBUS_RTU_MAX_ADU_SIZE (256U)
#define P5_MODBUS_RTU_MAX_FUNCTION_DATA_SIZE (252U)

typedef enum
{
  P5_MODBUS_ADU_OK = 0,
  P5_MODBUS_ADU_INVALID_ARGUMENT,
  P5_MODBUS_ADU_DATA_TOO_LARGE,
  P5_MODBUS_ADU_OUTPUT_TOO_SMALL,
  P5_MODBUS_ADU_FRAME_TOO_SHORT,
  P5_MODBUS_ADU_FRAME_TOO_LONG,
  P5_MODBUS_ADU_CRC_MISMATCH
} p5_modbus_adu_status_t;

typedef struct
{
  uint8_t address;
  uint8_t function;
  const uint8_t *data;
  size_t data_length;
} p5_modbus_adu_view_t;

p5_modbus_adu_status_t p5_modbus_rtu_adu_encode(
    uint8_t address,
    uint8_t function,
    const uint8_t *data,
    size_t data_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length);

p5_modbus_adu_status_t p5_modbus_rtu_adu_decode(
    const uint8_t *frame,
    size_t frame_length,
    p5_modbus_adu_view_t *view);

#endif
