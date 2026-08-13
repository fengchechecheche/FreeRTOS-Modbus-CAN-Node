#ifndef BSP_I2C_BUS_H
#define BSP_I2C_BUS_H

#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#define BSP_I2C_BUS_DEFAULT_TIMEOUT_MS UINT32_C(20)

typedef enum
{
  BSP_I2C_BUS_RESULT_OK = 0,
  BSP_I2C_BUS_RESULT_INVALID_ARGUMENT,
  BSP_I2C_BUS_RESULT_NOT_PRESENT,
  BSP_I2C_BUS_RESULT_BUSY,
  BSP_I2C_BUS_RESULT_TIMEOUT,
  BSP_I2C_BUS_RESULT_IO_ERROR
} bsp_i2c_bus_result_t;

I2C_HandleTypeDef *bsp_i2c_bus_handle(void);
bsp_i2c_bus_result_t bsp_i2c_bus_is_device_ready(uint8_t address_7bit,
                                                  uint32_t timeout_ms);
bsp_i2c_bus_result_t bsp_i2c_bus_read_register(uint8_t address_7bit,
                                                uint8_t register_address,
                                                uint8_t *data,
                                                size_t length,
                                                uint32_t timeout_ms);

#endif
