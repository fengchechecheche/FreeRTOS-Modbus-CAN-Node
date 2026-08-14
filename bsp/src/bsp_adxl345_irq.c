#include "bsp_adxl345_irq.h"

#include <stddef.h>

#include "main.h"
#include "stm32f4xx_hal.h"

static volatile bsp_adxl345_irq_diagnostics_t
    bsp_adxl345_irq_diagnostics;
static bsp_adxl345_irq_notifier_t bsp_adxl345_irq_notifier;

static uint32_t bsp_adxl345_saturating_increment(uint32_t value)
{
  return value == UINT32_MAX ? UINT32_MAX : value + 1U;
}

void bsp_adxl345_irq_initialize(void)
{
  bsp_adxl345_irq_diagnostics.data_ready_irq_count = 0U;
  bsp_adxl345_irq_diagnostics.wrong_pin_count = 0U;
  bsp_adxl345_irq_diagnostics.unregistered_notifier_count = 0U;
  bsp_adxl345_irq_notifier = NULL;
}

void bsp_adxl345_register_irq_notifier(
    bsp_adxl345_irq_notifier_t notifier)
{
  bsp_adxl345_irq_notifier = notifier;
}

bsp_adxl345_irq_diagnostics_t bsp_adxl345_get_irq_diagnostics(void)
{
  const bsp_adxl345_irq_diagnostics_t snapshot = {
      bsp_adxl345_irq_diagnostics.data_ready_irq_count,
      bsp_adxl345_irq_diagnostics.wrong_pin_count,
      bsp_adxl345_irq_diagnostics.unregistered_notifier_count,
  };
  return snapshot;
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
  if (gpio_pin != ADXL345_INT1_Pin)
  {
    bsp_adxl345_irq_diagnostics.wrong_pin_count =
        bsp_adxl345_saturating_increment(
            bsp_adxl345_irq_diagnostics.wrong_pin_count);
    return;
  }

  bsp_adxl345_irq_diagnostics.data_ready_irq_count =
      bsp_adxl345_saturating_increment(
          bsp_adxl345_irq_diagnostics.data_ready_irq_count);
  if (bsp_adxl345_irq_notifier == NULL)
  {
    bsp_adxl345_irq_diagnostics.unregistered_notifier_count =
        bsp_adxl345_saturating_increment(
            bsp_adxl345_irq_diagnostics.unregistered_notifier_count);
    return;
  }
  bsp_adxl345_irq_notifier();
}
