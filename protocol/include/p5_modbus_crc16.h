#ifndef P5_MODBUS_CRC16_H
#define P5_MODBUS_CRC16_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool p5_modbus_crc16_calculate(const uint8_t *data,
                                size_t length,
                                uint16_t *crc_out);

#endif
