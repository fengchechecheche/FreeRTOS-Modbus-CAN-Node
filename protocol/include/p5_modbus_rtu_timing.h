#ifndef P5_MODBUS_RTU_TIMING_H
#define P5_MODBUS_RTU_TIMING_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint32_t character_us;
  uint32_t inter_character_us;
  uint32_t inter_frame_us;
} p5_modbus_rtu_timing_t;

bool p5_modbus_rtu_timing_8e1(uint32_t baud,
                               p5_modbus_rtu_timing_t *timing_out);

#endif
