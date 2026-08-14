#include "p5_modbus_crc16.h"

bool p5_modbus_crc16_calculate(const uint8_t *data,
                                size_t length,
                                uint16_t *crc_out)
{
  if ((crc_out == NULL) || ((data == NULL) && (length != 0U)))
  {
    return false;
  }

  uint16_t crc = UINT16_C(0xFFFF);
  for (size_t index = 0U; index < length; ++index)
  {
    crc = (uint16_t)(crc ^ (uint16_t)data[index]);
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
      if ((crc & UINT16_C(0x0001)) != 0U)
      {
        crc = (uint16_t)((crc >> 1U) ^ UINT16_C(0xA001));
      }
      else
      {
        crc = (uint16_t)(crc >> 1U);
      }
    }
  }

  *crc_out = crc;
  return true;
}
