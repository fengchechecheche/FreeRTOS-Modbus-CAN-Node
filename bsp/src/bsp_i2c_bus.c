#include "bsp_i2c_bus.h"

#include <limits.h>

#include "i2c.h"

static bsp_i2c_bus_result_t bsp_i2c_bus_map_hal_status(
    HAL_StatusTypeDef status)
{
  if (status == HAL_OK)
  {
    return BSP_I2C_BUS_RESULT_OK;
  }
  if (status == HAL_TIMEOUT)
  {
    return BSP_I2C_BUS_RESULT_TIMEOUT;
  }
  if (status == HAL_BUSY)
  {
    return BSP_I2C_BUS_RESULT_BUSY;
  }
  if ((hi2c2.ErrorCode & HAL_I2C_ERROR_AF) != 0U)
  {
    return BSP_I2C_BUS_RESULT_NOT_PRESENT;
  }
  return BSP_I2C_BUS_RESULT_IO_ERROR;
}

I2C_HandleTypeDef *bsp_i2c_bus_handle(void)
{
  return &hi2c2;
}

bsp_i2c_bus_result_t bsp_i2c_bus_is_device_ready(uint8_t address_7bit,
                                                  uint32_t timeout_ms)
{
  if ((address_7bit == 0U) || (address_7bit > UINT8_C(0x7f)) ||
      (timeout_ms == 0U))
  {
    return BSP_I2C_BUS_RESULT_INVALID_ARGUMENT;
  }

  const HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(
      &hi2c2,
      (uint16_t)((uint16_t)address_7bit << 1U),
      1U,
      timeout_ms);
  return bsp_i2c_bus_map_hal_status(status);
}

bsp_i2c_bus_result_t bsp_i2c_bus_read_register(uint8_t address_7bit,
                                                uint8_t register_address,
                                                uint8_t *data,
                                                size_t length,
                                                uint32_t timeout_ms)
{
  if ((address_7bit == 0U) || (address_7bit > UINT8_C(0x7f)) ||
      (data == NULL) || (length == 0U) ||
      (length > BSP_I2C_BUS_MAX_TRANSFER_BYTES) ||
      (timeout_ms == 0U))
  {
    return BSP_I2C_BUS_RESULT_INVALID_ARGUMENT;
  }

  const HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
      &hi2c2,
      (uint16_t)((uint16_t)address_7bit << 1U),
      register_address,
      I2C_MEMADD_SIZE_8BIT,
      data,
      (uint16_t)length,
      timeout_ms);
  return bsp_i2c_bus_map_hal_status(status);
}

bsp_i2c_bus_result_t bsp_i2c_bus_write_register(uint8_t address_7bit,
                                                 uint8_t register_address,
                                                 const uint8_t *data,
                                                 size_t length,
                                                 uint32_t timeout_ms)
{
  if ((address_7bit == 0U) || (address_7bit > UINT8_C(0x7f)) ||
      (data == NULL) || (length == 0U) ||
      (length > BSP_I2C_BUS_MAX_TRANSFER_BYTES) ||
      (timeout_ms == 0U))
  {
    return BSP_I2C_BUS_RESULT_INVALID_ARGUMENT;
  }

  const HAL_StatusTypeDef status = HAL_I2C_Mem_Write(
      &hi2c2,
      (uint16_t)((uint16_t)address_7bit << 1U),
      register_address,
      I2C_MEMADD_SIZE_8BIT,
      (uint8_t *)data,
      (uint16_t)length,
      timeout_ms);
  return bsp_i2c_bus_map_hal_status(status);
}
