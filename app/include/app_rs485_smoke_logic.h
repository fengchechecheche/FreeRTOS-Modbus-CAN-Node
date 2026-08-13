#ifndef APP_RS485_SMOKE_LOGIC_H
#define APP_RS485_SMOKE_LOGIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

const uint8_t *app_rs485_smoke_request(size_t *length);
bool app_rs485_smoke_build_response(const uint8_t *request,
                                    size_t request_length,
                                    uint8_t *response,
                                    size_t response_capacity,
                                    size_t *response_length);

#endif
