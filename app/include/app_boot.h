#ifndef APP_BOOT_H
#define APP_BOOT_H

typedef enum
{
  APP_BOOT_OK = 0,
  APP_BOOT_ERROR = 1
} app_boot_status_t;

app_boot_status_t app_boot_initialize(void);
void app_boot_diagnostic_service(void);

#endif
