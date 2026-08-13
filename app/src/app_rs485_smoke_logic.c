#include "app_rs485_smoke_logic.h"

#include <string.h>

static const uint8_t app_rs485_request_bytes[] = {'P', '5', 'T', '0', '3'};
static const uint8_t app_rs485_response_bytes[] = {
    'P', '5', 'T', '0', '3', 'O', 'K'};

const uint8_t *app_rs485_smoke_request(size_t *length)
{
  if (length != NULL)
  {
    *length = sizeof(app_rs485_request_bytes);
  }
  return app_rs485_request_bytes;
}

bool app_rs485_smoke_build_response(const uint8_t *request,
                                    size_t request_length,
                                    uint8_t *response,
                                    size_t response_capacity,
                                    size_t *response_length)
{
  if (response_length != NULL)
  {
    *response_length = 0U;
  }

  if ((request == NULL) || (response == NULL) || (response_length == NULL) ||
      (request_length != sizeof(app_rs485_request_bytes)) ||
      (response_capacity < sizeof(app_rs485_response_bytes)) ||
      (memcmp(request, app_rs485_request_bytes, request_length) != 0))
  {
    return false;
  }

  (void)memcpy(response,
               app_rs485_response_bytes,
               sizeof(app_rs485_response_bytes));
  *response_length = sizeof(app_rs485_response_bytes);
  return true;
}
