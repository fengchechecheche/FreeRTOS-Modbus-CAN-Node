#include "p5_modbus_server.h"

#include <string.h>

#include "p5_modbus_crc16.h"

#define P5_MODBUS_SERVER_SERIAL_PROFILE_19200_8E1 (1U)

typedef enum
{
  P5_MODBUS_RESPONSE_NORMAL = 0,
  P5_MODBUS_RESPONSE_ILLEGAL_FUNCTION,
  P5_MODBUS_RESPONSE_ILLEGAL_DATA_ADDRESS,
  P5_MODBUS_RESPONSE_ILLEGAL_DATA_VALUE,
  P5_MODBUS_RESPONSE_SERVER_DEVICE_FAILURE
} p5_modbus_response_kind_t;

static uint32_t p5_modbus_server_increment(uint32_t value)
{
  return (value == UINT32_MAX) ? UINT32_MAX : (value + 1U);
}

static uint16_t p5_modbus_server_read_u16(const uint8_t *data)
{
  return (uint16_t)(((uint16_t)data[0] << 8U) | (uint16_t)data[1]);
}

static void p5_modbus_server_write_u16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)(value >> 8U);
  data[1] = (uint8_t)(value & UINT16_C(0x00ff));
}

static bool p5_modbus_server_config_is_valid(
    const p5_modbus_server_config_t *config)
{
  return (config != NULL) && (config->input_provider != NULL) &&
         (config->tx_sink != NULL) && (config->response_buffer != NULL) &&
         (config->response_capacity >= P5_MODBUS_SERVER_MAX_RESPONSE_SIZE) &&
         (config->input_registers != NULL) &&
         (config->input_register_capacity >=
          P5_MODBUS_SERVER_INPUT_REGISTER_COUNT);
}

static void p5_modbus_server_note_response(
    p5_modbus_server_t *server,
    p5_modbus_response_kind_t kind)
{
  switch (kind)
  {
    case P5_MODBUS_RESPONSE_NORMAL:
      server->diagnostics.normal_responses = p5_modbus_server_increment(
          server->diagnostics.normal_responses);
      break;
    case P5_MODBUS_RESPONSE_ILLEGAL_FUNCTION:
      server->diagnostics.illegal_function_exceptions =
          p5_modbus_server_increment(
              server->diagnostics.illegal_function_exceptions);
      break;
    case P5_MODBUS_RESPONSE_ILLEGAL_DATA_ADDRESS:
      server->diagnostics.illegal_data_address_exceptions =
          p5_modbus_server_increment(
              server->diagnostics.illegal_data_address_exceptions);
      break;
    case P5_MODBUS_RESPONSE_ILLEGAL_DATA_VALUE:
      server->diagnostics.illegal_data_value_exceptions =
          p5_modbus_server_increment(
              server->diagnostics.illegal_data_value_exceptions);
      break;
    case P5_MODBUS_RESPONSE_SERVER_DEVICE_FAILURE:
      server->diagnostics.server_device_failure_exceptions =
          p5_modbus_server_increment(
              server->diagnostics.server_device_failure_exceptions);
      break;
    default:
      break;
  }
}

static p5_modbus_server_process_result_t p5_modbus_server_send(
    p5_modbus_server_t *server,
    size_t length_without_crc,
    p5_modbus_response_kind_t kind)
{
  if ((length_without_crc + 2U) > server->config.response_capacity)
  {
    server->diagnostics.tx_rejected = p5_modbus_server_increment(
        server->diagnostics.tx_rejected);
    return P5_MODBUS_SERVER_PROCESS_RESPONSE_REJECTED;
  }

  uint16_t crc = 0U;
  if (!p5_modbus_crc16_calculate(server->config.response_buffer,
                                  length_without_crc,
                                  &crc))
  {
    server->diagnostics.tx_rejected = p5_modbus_server_increment(
        server->diagnostics.tx_rejected);
    return P5_MODBUS_SERVER_PROCESS_RESPONSE_REJECTED;
  }
  server->config.response_buffer[length_without_crc] =
      (uint8_t)(crc & UINT16_C(0x00ff));
  server->config.response_buffer[length_without_crc + 1U] =
      (uint8_t)(crc >> 8U);

  const size_t frame_length = length_without_crc + 2U;
  if (!server->config.tx_sink(server->config.tx_context,
                              server->config.response_buffer,
                              frame_length))
  {
    server->diagnostics.tx_rejected = p5_modbus_server_increment(
        server->diagnostics.tx_rejected);
    return P5_MODBUS_SERVER_PROCESS_RESPONSE_REJECTED;
  }

  p5_modbus_server_note_response(server, kind);
  return P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED;
}

static p5_modbus_server_process_result_t p5_modbus_server_exception(
    p5_modbus_server_t *server,
    uint8_t function,
    uint8_t exception,
    p5_modbus_response_kind_t kind)
{
  server->config.response_buffer[0] = server->active_address;
  server->config.response_buffer[1] = (uint8_t)(function | UINT8_C(0x80));
  server->config.response_buffer[2] = exception;
  return p5_modbus_server_send(server, 3U, kind);
}

static bool p5_modbus_server_read_request(
    const p5_modbus_adu_view_t *request,
    uint16_t *start,
    uint16_t *quantity)
{
  if ((request->data == NULL) || (request->data_length != 4U) ||
      (start == NULL) || (quantity == NULL))
  {
    return false;
  }
  *start = p5_modbus_server_read_u16(request->data);
  *quantity = p5_modbus_server_read_u16(&request->data[2]);
  return true;
}

static bool p5_modbus_server_span_is_valid(uint16_t start,
                                            uint16_t quantity,
                                            uint16_t register_count)
{
  return (start < register_count) &&
         (quantity <= (uint16_t)(register_count - start));
}

static uint16_t p5_modbus_server_holding_value(
    const p5_modbus_server_t *server,
    uint16_t address)
{
  switch (address)
  {
    case 0U:
      return server->active_address;
    case 1U:
      return P5_MODBUS_SERVER_DEFAULT_ADDRESS;
    case 2U:
      return P5_MODBUS_SERVER_SERIAL_PROFILE_19200_8E1;
    case 3U:
      return server->configuration_generation;
    default:
      return 0U;
  }
}

static p5_modbus_server_process_result_t p5_modbus_server_read_holding(
    p5_modbus_server_t *server,
    const p5_modbus_adu_view_t *request)
{
  uint16_t start = 0U;
  uint16_t quantity = 0U;
  if (!p5_modbus_server_read_request(request, &start, &quantity))
  {
    return p5_modbus_server_exception(
        server,
        request->function,
        P5_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
        P5_MODBUS_RESPONSE_ILLEGAL_DATA_VALUE);
  }
  if ((quantity == 0U) ||
      (quantity > P5_MODBUS_SERVER_MAX_READ_REGISTERS))
  {
    return p5_modbus_server_exception(
        server,
        request->function,
        P5_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
        P5_MODBUS_RESPONSE_ILLEGAL_DATA_VALUE);
  }
  if (!p5_modbus_server_span_is_valid(
          start, quantity, P5_MODBUS_SERVER_HOLDING_REGISTER_COUNT))
  {
    return p5_modbus_server_exception(
        server,
        request->function,
        P5_MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
        P5_MODBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
  }

  uint8_t *response = server->config.response_buffer;
  response[0] = server->active_address;
  response[1] = request->function;
  response[2] = (uint8_t)(quantity * 2U);
  for (uint16_t index = 0U; index < quantity; ++index)
  {
    p5_modbus_server_write_u16(
        &response[3U + ((size_t)index * 2U)],
        p5_modbus_server_holding_value(server,
                                       (uint16_t)(start + index)));
  }
  return p5_modbus_server_send(
      server, 3U + ((size_t)quantity * 2U), P5_MODBUS_RESPONSE_NORMAL);
}

static p5_modbus_server_process_result_t p5_modbus_server_read_input(
    p5_modbus_server_t *server,
    const p5_modbus_adu_view_t *request)
{
  uint16_t start = 0U;
  uint16_t quantity = 0U;
  if (!p5_modbus_server_read_request(request, &start, &quantity))
  {
    return p5_modbus_server_exception(
        server,
        request->function,
        P5_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
        P5_MODBUS_RESPONSE_ILLEGAL_DATA_VALUE);
  }
  if ((quantity == 0U) ||
      (quantity > P5_MODBUS_SERVER_MAX_READ_REGISTERS))
  {
    return p5_modbus_server_exception(
        server,
        request->function,
        P5_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
        P5_MODBUS_RESPONSE_ILLEGAL_DATA_VALUE);
  }
  if (!p5_modbus_server_span_is_valid(
          start, quantity, P5_MODBUS_SERVER_INPUT_REGISTER_COUNT))
  {
    return p5_modbus_server_exception(
        server,
        request->function,
        P5_MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
        P5_MODBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
  }

  (void)memset(server->config.input_registers,
               0,
               P5_MODBUS_SERVER_INPUT_REGISTER_COUNT * sizeof(uint16_t));
  if (!server->config.input_provider(
          server->config.input_context,
          server->config.input_registers,
          P5_MODBUS_SERVER_INPUT_REGISTER_COUNT))
  {
    server->diagnostics.input_image_unavailable =
        p5_modbus_server_increment(
            server->diagnostics.input_image_unavailable);
    return p5_modbus_server_exception(
        server,
        request->function,
        P5_MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE,
        P5_MODBUS_RESPONSE_SERVER_DEVICE_FAILURE);
  }

  uint8_t *response = server->config.response_buffer;
  response[0] = server->active_address;
  response[1] = request->function;
  response[2] = (uint8_t)(quantity * 2U);
  for (uint16_t index = 0U; index < quantity; ++index)
  {
    p5_modbus_server_write_u16(
        &response[3U + ((size_t)index * 2U)],
        server->config.input_registers[start + index]);
  }
  return p5_modbus_server_send(
      server, 3U + ((size_t)quantity * 2U), P5_MODBUS_RESPONSE_NORMAL);
}

static p5_modbus_server_process_result_t p5_modbus_server_write_single(
    p5_modbus_server_t *server,
    const p5_modbus_adu_view_t *request)
{
  if ((request->data == NULL) || (request->data_length != 4U))
  {
    return p5_modbus_server_exception(
        server,
        request->function,
        P5_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
        P5_MODBUS_RESPONSE_ILLEGAL_DATA_VALUE);
  }

  const uint16_t address = p5_modbus_server_read_u16(request->data);
  const uint16_t value = p5_modbus_server_read_u16(&request->data[2]);
  if (address != 0U)
  {
    return p5_modbus_server_exception(
        server,
        request->function,
        P5_MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
        P5_MODBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
  }
  if ((value < P5_MODBUS_SERVER_MIN_ADDRESS) ||
      (value > P5_MODBUS_SERVER_MAX_ADDRESS))
  {
    return p5_modbus_server_exception(
        server,
        request->function,
        P5_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
        P5_MODBUS_RESPONSE_ILLEGAL_DATA_VALUE);
  }
  if (server->pending_address_valid &&
      (value != server->active_address))
  {
    server->diagnostics.tx_rejected = p5_modbus_server_increment(
        server->diagnostics.tx_rejected);
    return P5_MODBUS_SERVER_PROCESS_RESPONSE_REJECTED;
  }

  uint8_t *response = server->config.response_buffer;
  response[0] = server->active_address;
  response[1] = request->function;
  (void)memcpy(&response[2], request->data, 4U);
  const p5_modbus_server_process_result_t result =
      p5_modbus_server_send(server, 6U, P5_MODBUS_RESPONSE_NORMAL);
  if (result != P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED)
  {
    return result;
  }

  if (value == server->active_address)
  {
    server->diagnostics.idempotent_address_writes =
        p5_modbus_server_increment(
            server->diagnostics.idempotent_address_writes);
    return result;
  }

  server->pending_address = (uint8_t)value;
  server->pending_address_valid = true;
  server->diagnostics.address_writes_staged = p5_modbus_server_increment(
      server->diagnostics.address_writes_staged);
  return result;
}

bool p5_modbus_server_initialize(
    p5_modbus_server_t *server,
    const p5_modbus_server_config_t *config)
{
  if ((server == NULL) || !p5_modbus_server_config_is_valid(config))
  {
    return false;
  }
  (void)memset(server, 0, sizeof(*server));
  server->config = *config;
  server->active_address = P5_MODBUS_SERVER_DEFAULT_ADDRESS;
  return true;
}

p5_modbus_server_process_result_t p5_modbus_server_process(
    p5_modbus_server_t *server,
    const p5_modbus_adu_view_t *request)
{
  if ((server == NULL) || (request == NULL) ||
      ((request->data == NULL) && (request->data_length != 0U)))
  {
    return P5_MODBUS_SERVER_PROCESS_INVALID_ARGUMENT;
  }

  if (request->address == 0U)
  {
    server->diagnostics.broadcast_ignored = p5_modbus_server_increment(
        server->diagnostics.broadcast_ignored);
    return P5_MODBUS_SERVER_PROCESS_IGNORED;
  }
  if (request->address != server->active_address)
  {
    server->diagnostics.foreign_ignored = p5_modbus_server_increment(
        server->diagnostics.foreign_ignored);
    return P5_MODBUS_SERVER_PROCESS_IGNORED;
  }

  server->diagnostics.addressed_requests = p5_modbus_server_increment(
      server->diagnostics.addressed_requests);
  switch (request->function)
  {
    case P5_MODBUS_FUNCTION_READ_HOLDING_REGISTERS:
      return p5_modbus_server_read_holding(server, request);
    case P5_MODBUS_FUNCTION_READ_INPUT_REGISTERS:
      return p5_modbus_server_read_input(server, request);
    case P5_MODBUS_FUNCTION_WRITE_SINGLE_REGISTER:
      return p5_modbus_server_write_single(server, request);
    default:
      return p5_modbus_server_exception(
          server,
          request->function,
          P5_MODBUS_EXCEPTION_ILLEGAL_FUNCTION,
          P5_MODBUS_RESPONSE_ILLEGAL_FUNCTION);
  }
}

void p5_modbus_server_on_tx_complete(p5_modbus_server_t *server)
{
  if ((server == NULL) || !server->pending_address_valid)
  {
    return;
  }
  server->active_address = server->pending_address;
  server->pending_address = 0U;
  server->pending_address_valid = false;
  server->configuration_generation =
      (uint16_t)(server->configuration_generation + 1U);
  server->diagnostics.address_writes_committed =
      p5_modbus_server_increment(
          server->diagnostics.address_writes_committed);
}

void p5_modbus_server_on_link_failure(p5_modbus_server_t *server)
{
  if ((server == NULL) || !server->pending_address_valid)
  {
    return;
  }
  server->pending_address = 0U;
  server->pending_address_valid = false;
  server->diagnostics.address_writes_cancelled =
      p5_modbus_server_increment(
          server->diagnostics.address_writes_cancelled);
}

uint8_t p5_modbus_server_active_address(
    const p5_modbus_server_t *server)
{
  return (server == NULL) ? 0U : server->active_address;
}

uint16_t p5_modbus_server_configuration_generation(
    const p5_modbus_server_t *server)
{
  return (server == NULL) ? 0U : server->configuration_generation;
}

bool p5_modbus_server_has_pending_write(
    const p5_modbus_server_t *server)
{
  return (server != NULL) && server->pending_address_valid;
}

p5_modbus_server_diagnostics_t p5_modbus_server_diagnostics(
    const p5_modbus_server_t *server)
{
  if (server == NULL)
  {
    return (p5_modbus_server_diagnostics_t){0};
  }
  return server->diagnostics;
}
