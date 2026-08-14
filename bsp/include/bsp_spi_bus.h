#ifndef BSP_SPI_BUS_H
#define BSP_SPI_BUS_H

#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#define BSP_SPI_BUS_DEFAULT_TIMEOUT_MS UINT32_C(20)
#define BSP_SPI_BUS_MAX_TRANSFER_BYTES (32U)

typedef enum
{
  BSP_SPI_DEVICE_BME280 = 0,
  BSP_SPI_DEVICE_ADXL345
} bsp_spi_device_t;

typedef enum
{
  BSP_SPI_BUS_RESULT_OK = 0,
  BSP_SPI_BUS_RESULT_INVALID_ARGUMENT,
  BSP_SPI_BUS_RESULT_BUSY,
  BSP_SPI_BUS_RESULT_TIMEOUT,
  BSP_SPI_BUS_RESULT_IO_ERROR
} bsp_spi_bus_result_t;

SPI_HandleTypeDef *bsp_spi_bus_handle(void);
bsp_spi_bus_result_t bsp_spi_bus_read_register(bsp_spi_device_t device,
                                                uint8_t register_address,
                                                uint8_t *value,
                                                uint32_t timeout_ms);
bsp_spi_bus_result_t bsp_spi_bus_read_registers(bsp_spi_device_t device,
                                                 uint8_t register_address,
                                                 uint8_t *data,
                                                 size_t length,
                                                 uint32_t timeout_ms);
bsp_spi_bus_result_t bsp_spi_bus_write_register(bsp_spi_device_t device,
                                                 uint8_t register_address,
                                                 uint8_t value,
                                                 uint32_t timeout_ms);

#endif
