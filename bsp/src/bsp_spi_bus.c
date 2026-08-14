#include "bsp_spi_bus.h"

#include <stdbool.h>
#include <string.h>

#include "main.h"
#include "spi.h"

#define BSP_SPI_BUS_READ_BIT UINT8_C(0x80)
#define BSP_SPI_BUS_ADXL345_MULTIBYTE_BIT UINT8_C(0x40)
#define BSP_SPI_BUS_ADXL345_DATAX0_REGISTER UINT8_C(0x32)

_Static_assert(
    (BSP_SPI_BUS_ADXL345_DATAX0_REGISTER | BSP_SPI_BUS_READ_BIT |
     BSP_SPI_BUS_ADXL345_MULTIBYTE_BIT) == UINT8_C(0xf2),
    "ADXL345 coherent XYZ command must remain 0xf2");

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
  return bsp_spi_bus_read_registers(
      device, register_address, value, 1U, timeout_ms);
}

bsp_spi_bus_result_t bsp_spi_bus_read_registers(bsp_spi_device_t device,
                                                 uint8_t register_address,
                                                 uint8_t *data,
                                                 size_t length,
                                                 uint32_t timeout_ms)
{
  if ((data == NULL) || (length == 0U) ||
      (length > BSP_SPI_BUS_MAX_TRANSFER_BYTES) || (timeout_ms == 0U))
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

  uint8_t tx[BSP_SPI_BUS_MAX_TRANSFER_BYTES + 1U];
  uint8_t rx[BSP_SPI_BUS_MAX_TRANSFER_BYTES + 1U];
  tx[0] = (uint8_t)((register_address & UINT8_C(0x7f)) |
                    BSP_SPI_BUS_READ_BIT);
  if ((device == BSP_SPI_DEVICE_ADXL345) && (length > 1U))
  {
    tx[0] |= BSP_SPI_BUS_ADXL345_MULTIBYTE_BIT;
  }
  (void)memset(&tx[1], UINT8_C(0xff), length);
  (void)memset(rx, 0, length + 1U);
  const HAL_StatusTypeDef status =
      HAL_SPI_TransmitReceive(
          &hspi1, tx, rx, (uint16_t)(length + 1U), timeout_ms);

  bsp_spi_bus_deselect_all();
  bsp_spi_bus_busy = false;
  const bsp_spi_bus_result_t result = bsp_spi_bus_map_hal_status(status);
  if (result == BSP_SPI_BUS_RESULT_OK)
  {
    (void)memcpy(data, &rx[1], length);
  }
  return result;
}

bsp_spi_bus_result_t bsp_spi_bus_write_register(bsp_spi_device_t device,
                                                 uint8_t register_address,
                                                 uint8_t value,
                                                 uint32_t timeout_ms)
{
  if (timeout_ms == 0U)
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

  uint8_t tx[2] = {
      (uint8_t)(register_address & UINT8_C(0x7f)), value};
  const HAL_StatusTypeDef status =
      HAL_SPI_Transmit(&hspi1, tx, (uint16_t)sizeof(tx), timeout_ms);
  bsp_spi_bus_deselect_all();
  bsp_spi_bus_busy = false;
  return bsp_spi_bus_map_hal_status(status);
}
