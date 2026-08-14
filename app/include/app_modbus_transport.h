#ifndef APP_MODBUS_TRANSPORT_H
#define APP_MODBUS_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "p5_modbus_rtu_stream.h"

typedef struct
{
  uint32_t rx_chunks;
  uint32_t rx_bytes;
  uint32_t idle_chunks;
  uint32_t dma_complete_chunks;
  uint32_t invalid_chunks;
  uint32_t unhandled_valid_frames;
  p5_modbus_rtu_stream_counters_t stream;
} app_modbus_transport_diagnostics_t;

bool app_modbus_transport_initialize(void);
void app_modbus_transport_service_received(void);
void app_modbus_transport_poll(void);
void app_modbus_transport_reset_partial(void);
bool app_modbus_transport_has_partial_frame(void);
app_modbus_transport_diagnostics_t app_modbus_transport_get_diagnostics(void);

#endif
