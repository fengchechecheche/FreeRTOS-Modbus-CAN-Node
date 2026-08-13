#include "bsp_i2c_bus.h"

#include "i2c.h"

I2C_HandleTypeDef *bsp_i2c_bus_handle(void)
{
  return &hi2c2;
}
