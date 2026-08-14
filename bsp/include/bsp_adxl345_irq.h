#ifndef BSP_ADXL345_IRQ_H
#define BSP_ADXL345_IRQ_H

#include <stdint.h>

typedef void (*bsp_adxl345_irq_notifier_t)(void);

typedef struct
{
  uint32_t data_ready_irq_count;
  uint32_t wrong_pin_count;
  uint32_t unregistered_notifier_count;
} bsp_adxl345_irq_diagnostics_t;

void bsp_adxl345_irq_initialize(void);
void bsp_adxl345_register_irq_notifier(
    bsp_adxl345_irq_notifier_t notifier);
bsp_adxl345_irq_diagnostics_t bsp_adxl345_get_irq_diagnostics(void);

#endif
