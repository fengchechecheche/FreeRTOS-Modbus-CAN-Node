#ifndef BSP_WATCHDOG_H
#define BSP_WATCHDOG_H

#include <stdbool.h>

void bsp_watchdog_enable_debug_freeze(void);
bool bsp_watchdog_refresh(void);

#endif
