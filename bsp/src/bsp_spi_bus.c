#include "bsp_spi_bus.h"

#include <stdbool.h>

#include "main.h"
#include "spi.h"

static bool bsp_spi_bus_busy;

static void bsp_spi_bus_deselect_all(void)
{
  HAL_GPIO_WritePin(BME280_CS_GPIO_Port, BME280_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(ADXL345_CS_GPIO_Port, ADXL345_CS_Pin, GPIO_PIN_SET);
}

static bool bsp_spi_bus_select(bsp_spi_device_t device)
{
  bsp_spi_bus_deselect_all();
  switch (device)
  {
    case BSP_SPI_DEVICE_BME280:
      HAL_GPIO_WritePin(BME280_CS_GPIO_Port,
                        BME280_CS_Pin,
                        GPIO_PIN_RESET);
      return true;
    case BSP_SPI_DEVICE_ADXL345:
      HAL_GPIO_WritePin(ADXL345_CS_GPIO_Port,
                        ADXL345_CS_Pin,
                        GPIO_PIN_RESET);
      return true;
    default:
      return false;
  }
}

static bsp_spi_bus_result_t bsp_spi_bus_map_hal_status(
    HAL_StatusTypeDef status)
{
  switch (status)
  {
    case HAL_OK:
      return BSP_SPI_BUS_RESULT_OK;
    case HAL_BUSY:
      return BSP_SPI_BUS_RESULT_BUSY;
    case HAL_TIMEOUT:
      return BSP_SPI_BUS_RESULT_TIMEOUT;
    case HAL_ERROR:
    default:
      return BSP_SPI_BUS_RESULT_IO_ERROR;
  }
}

SPI_HandleTypeDef *bsp_spi_bus_handle(void)
{
  return &hspi1;
}

bsp_spi_bus_result_t bsp_spi_bus_read_register(bsp_spi_device_t device,
                                                uint8_t register_address,
                                                uint8_t *value,
                                                uint32_t timeout_ms)
{
  if ((value == NULL) || (timeout_ms == 0U))
  {
    return BSP_SPI_BUS_RESULT_INVALID_ARGUMENT;
  }
  if (bsp_spi_bus_busy)
  {
    return BSP_SPI_BUS_RESULT_BUSY;
  }

  bsp_spi_bus_busy = true;
  if (!bsp_spi_bus_select(device))
  {
    bsp_spi_bus_deselect_all();
    bsp_spi_bus_busy = false;
    return BSP_SPI_BUS_RESULT_INVALID_ARGUMENT;
  }

  uint8_t tx[2] = {(uint8_t)((register_address & UINT8_C(0x7f)) |
                             UINT8_C(0x80)),
                   UINT8_C(0xff)};
  uint8_t rx[2] = {0U, 0U};
  const HAL_StatusTypeDef status =
      HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2U, timeout_ms);

  bsp_spi_bus_deselect_all();
  bsp_spi_bus_busy = false;
  const bsp_spi_bus_result_t result = bsp_spi_bus_map_hal_status(status);
  if (result == BSP_SPI_BUS_RESULT_OK)
  {
    *value = rx[1];
  }
  return result;
}
