#include "p5_modbus_rtu_adu.h"

#include <string.h>

#include "p5_modbus_crc16.h"

static void p5_modbus_adu_clear_view(p5_modbus_adu_view_t *view)
{
  if (view != NULL)
  {
    view->address = 0U;
    view->function = 0U;
    view->data = NULL;
    view->data_length = 0U;
  }
}

p5_modbus_adu_status_t p5_modbus_rtu_adu_encode(
    uint8_t address,
    uint8_t function,
    const uint8_t *data,
    size_t data_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length)
{
  if (output_length == NULL)
  {
    return P5_MODBUS_ADU_INVALID_ARGUMENT;
  }
  *output_length = 0U;

  if ((output == NULL) || ((data == NULL) && (data_length != 0U)))
  {
    return P5_MODBUS_ADU_INVALID_ARGUMENT;
  }
  if (data_length > P5_MODBUS_RTU_MAX_FUNCTION_DATA_SIZE)
  {
    return P5_MODBUS_ADU_DATA_TOO_LARGE;
  }

  const size_t required = data_length + P5_MODBUS_RTU_MIN_ADU_SIZE;
  if (output_capacity < required)
  {
    return P5_MODBUS_ADU_OUTPUT_TOO_SMALL;
  }

  output[0] = address;
  output[1] = function;
  if (data_length != 0U)
  {
    (void)memcpy(&output[2], data, data_length);
  }

  uint16_t crc = 0U;
  if (!p5_modbus_crc16_calculate(output, required - 2U, &crc))
  {
    return P5_MODBUS_ADU_INVALID_ARGUMENT;
  }
  output[required - 2U] = (uint8_t)(crc & UINT16_C(0x00FF));
  output[required - 1U] = (uint8_t)(crc >> 8U);
  *output_length = required;
  return P5_MODBUS_ADU_OK;
}

p5_modbus_adu_status_t p5_modbus_rtu_adu_decode(
    const uint8_t *frame,
    size_t frame_length,
    p5_modbus_adu_view_t *view)
{
  p5_modbus_adu_clear_view(view);
  if ((frame == NULL) || (view == NULL))
  {
    return P5_MODBUS_ADU_INVALID_ARGUMENT;
  }
  if (frame_length < P5_MODBUS_RTU_MIN_ADU_SIZE)
  {
    return P5_MODBUS_ADU_FRAME_TOO_SHORT;
  }
  if (frame_length > P5_MODBUS_RTU_MAX_ADU_SIZE)
  {
    return P5_MODBUS_ADU_FRAME_TOO_LONG;
  }

  uint16_t calculated_crc = 0U;
  if (!p5_modbus_crc16_calculate(frame,
                                  frame_length - 2U,
                                  &calculated_crc))
  {
    return P5_MODBUS_ADU_INVALID_ARGUMENT;
  }
  const uint16_t wire_crc =
      (uint16_t)((uint16_t)frame[frame_length - 2U] |
                 (uint16_t)((uint16_t)frame[frame_length - 1U] << 8U));
  if (wire_crc != calculated_crc)
  {
    return P5_MODBUS_ADU_CRC_MISMATCH;
  }

  view->address = frame[0];
  view->function = frame[1];
  view->data = &frame[2];
  view->data_length = frame_length - P5_MODBUS_RTU_MIN_ADU_SIZE;
  return P5_MODBUS_ADU_OK;
}
