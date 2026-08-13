#include "bsp_spi_bus.h"

#include "spi.h"

SPI_HandleTypeDef *bsp_spi_bus_handle(void)
{
  return &hspi1;
}
