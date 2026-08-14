#ifndef P5_MODBUS_SERVER_H
#define P5_MODBUS_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "p5_modbus_rtu_adu.h"

#define P5_MODBUS_SERVER_DEFAULT_ADDRESS (4U)
#define P5_MODBUS_SERVER_MIN_ADDRESS (1U)
#define P5_MODBUS_SERVER_MAX_ADDRESS (247U)
#define P5_MODBUS_SERVER_HOLDING_REGISTER_COUNT (4U)
#define P5_MODBUS_SERVER_INPUT_REGISTER_COUNT (122U)
#define P5_MODBUS_SERVER_MAX_READ_REGISTERS (125U)
#define P5_MODBUS_SERVER_MAX_RESPONSE_SIZE (249U)

#define P5_MODBUS_FUNCTION_READ_HOLDING_REGISTERS (0x03U)
#define P5_MODBUS_FUNCTION_READ_INPUT_REGISTERS (0x04U)
#define P5_MODBUS_FUNCTION_WRITE_SINGLE_REGISTER (0x06U)

#define P5_MODBUS_EXCEPTION_ILLEGAL_FUNCTION (0x01U)
#define P5_MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS (0x02U)
#define P5_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE (0x03U)
#define P5_MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE (0x04U)

typedef bool (*p5_modbus_server_input_provider_t)(
    void *context,
    uint16_t *registers,
    size_t register_count);

typedef bool (*p5_modbus_server_tx_sink_t)(
    void *context,
    const uint8_t *frame,
    size_t frame_length);

typedef struct
{
  void *input_context;
  p5_modbus_server_input_provider_t input_provider;
  void *tx_context;
  p5_modbus_server_tx_sink_t tx_sink;
  uint8_t *response_buffer;
  size_t response_capacity;
  uint16_t *input_registers;
  size_t input_register_capacity;
} p5_modbus_server_config_t;

typedef struct
{
  uint32_t addressed_requests;
  uint32_t foreign_ignored;
  uint32_t broadcast_ignored;
  uint32_t normal_responses;
  uint32_t illegal_function_exceptions;
  uint32_t illegal_data_address_exceptions;
  uint32_t illegal_data_value_exceptions;
  uint32_t server_device_failure_exceptions;
  uint32_t input_image_unavailable;
  uint32_t tx_rejected;
  uint32_t address_writes_staged;
  uint32_t address_writes_committed;
  uint32_t address_writes_cancelled;
  uint32_t idempotent_address_writes;
} p5_modbus_server_diagnostics_t;

typedef enum
{
  P5_MODBUS_SERVER_PROCESS_INVALID_ARGUMENT = 0,
  P5_MODBUS_SERVER_PROCESS_IGNORED,
  P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED,
  P5_MODBUS_SERVER_PROCESS_RESPONSE_REJECTED
} p5_modbus_server_process_result_t;

typedef struct
{
  p5_modbus_server_config_t config;
  p5_modbus_server_diagnostics_t diagnostics;
  uint8_t active_address;
  uint8_t pending_address;
  uint16_t configuration_generation;
  bool pending_address_valid;
} p5_modbus_server_t;

bool p5_modbus_server_initialize(
    p5_modbus_server_t *server,
    const p5_modbus_server_config_t *config);
p5_modbus_server_process_result_t p5_modbus_server_process(
    p5_modbus_server_t *server,
    const p5_modbus_adu_view_t *request);
void p5_modbus_server_on_tx_complete(p5_modbus_server_t *server);
void p5_modbus_server_on_link_failure(p5_modbus_server_t *server);
uint8_t p5_modbus_server_active_address(
    const p5_modbus_server_t *server);
uint16_t p5_modbus_server_configuration_generation(
    const p5_modbus_server_t *server);
bool p5_modbus_server_has_pending_write(
    const p5_modbus_server_t *server);
p5_modbus_server_diagnostics_t p5_modbus_server_diagnostics(
    const p5_modbus_server_t *server);

#endif
