#ifndef APP_DEVICE_PROBE_H
#define APP_DEVICE_PROBE_H

#include "app_device_probe_logic.h"

void app_device_probe_initialize(void);
void app_device_probe_run_once(void);
const app_device_probe_summary_t *app_device_probe_get_summary(void);

#endif
