#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "p5_modbus_rtu_adu.h"
#include "p5_modbus_server.h"

#define CHECK(condition)                                                     \
  do                                                                         \
  {                                                                          \
    if (!(condition))                                                        \
    {                                                                        \
      (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",                 \
                    __FILE__, __LINE__, #condition);                         \
      return false;                                                          \
    }                                                                        \
  } while (0)

typedef struct
{
  bool accept;
  uint8_t frame[P5_MODBUS_RTU_MAX_ADU_SIZE];
  size_t length;
  uint32_t calls;
} tx_capture_t;

typedef struct
{
  bool available;
  uint16_t image[P5_MODBUS_SERVER_INPUT_REGISTER_COUNT];
  uint32_t calls;
} input_fixture_t;

typedef struct
{
  p5_modbus_server_t server;
  uint8_t response[P5_MODBUS_RTU_MAX_ADU_SIZE];
  uint16_t image_scratch[P5_MODBUS_SERVER_INPUT_REGISTER_COUNT];
  tx_capture_t tx;
  input_fixture_t input;
} fixture_t;

static bool capture_tx(void *context,
                       const uint8_t *frame,
                       size_t frame_length)
{
  tx_capture_t *capture = context;
  ++capture->calls;
  if (!capture->accept || (frame_length > sizeof(capture->frame)))
  {
    return false;
  }
  (void)memcpy(capture->frame, frame, frame_length);
  capture->length = frame_length;
  return true;
}

static bool provide_input(void *context,
                          uint16_t *registers,
                          size_t register_count)
{
  input_fixture_t *input = context;
  ++input->calls;
  if (!input->available ||
      (register_count != P5_MODBUS_SERVER_INPUT_REGISTER_COUNT))
  {
    return false;
  }
  (void)memcpy(registers, input->image, sizeof(input->image));
  return true;
}

static bool fixture_initialize(fixture_t *fixture)
{
  (void)memset(fixture, 0, sizeof(*fixture));
  fixture->tx.accept = true;
  fixture->input.available = true;
  for (size_t index = 0U;
       index < P5_MODBUS_SERVER_INPUT_REGISTER_COUNT;
       ++index)
  {
    fixture->input.image[index] = (uint16_t)(UINT16_C(0x1000) + index);
  }
  const p5_modbus_server_config_t config = {
      .input_context = &fixture->input,
      .input_provider = provide_input,
      .tx_context = &fixture->tx,
      .tx_sink = capture_tx,
      .response_buffer = fixture->response,
      .response_capacity = sizeof(fixture->response),
      .input_registers = fixture->image_scratch,
      .input_register_capacity = P5_MODBUS_SERVER_INPUT_REGISTER_COUNT,
  };
  return p5_modbus_server_initialize(&fixture->server, &config);
}

static p5_modbus_adu_view_t make_request(uint8_t address,
                                         uint8_t function,
                                         const uint8_t *data,
                                         size_t data_length)
{
  const p5_modbus_adu_view_t request = {
      .address = address,
      .function = function,
      .data = data,
      .data_length = data_length,
  };
  return request;
}

static bool decode_response(const fixture_t *fixture,
                            p5_modbus_adu_view_t *view)
{
  return p5_modbus_rtu_adu_decode(
             fixture->tx.frame, fixture->tx.length, view) ==
         P5_MODBUS_ADU_OK;
}

static bool response_is_exception(const fixture_t *fixture,
                                  uint8_t request_function,
                                  uint8_t exception)
{
  p5_modbus_adu_view_t response;
  CHECK(decode_response(fixture, &response));
  CHECK(response.address == P5_MODBUS_SERVER_DEFAULT_ADDRESS);
  CHECK(response.function == (uint8_t)(request_function | UINT8_C(0x80)));
  CHECK(response.data_length == 1U);
  CHECK(response.data[0] == exception);
  return true;
}

static bool test_address_filter(void)
{
  fixture_t fixture;
  CHECK(fixture_initialize(&fixture));
  const uint8_t read[] = {0U, 0U, 0U, 1U};
  p5_modbus_adu_view_t request = make_request(
      0U, P5_MODBUS_FUNCTION_READ_HOLDING_REGISTERS, read, sizeof(read));
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_IGNORED);
  request.address = 3U;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_IGNORED);
  CHECK(fixture.tx.calls == 0U);
  const p5_modbus_server_diagnostics_t diagnostics =
      p5_modbus_server_diagnostics(&fixture.server);
  CHECK(diagnostics.broadcast_ignored == 1U);
  CHECK(diagnostics.foreign_ignored == 1U);
  CHECK(diagnostics.addressed_requests == 0U);
  return true;
}

static bool test_holding_and_exceptions(void)
{
  fixture_t fixture;
  CHECK(fixture_initialize(&fixture));
  uint8_t read[] = {0U, 0U, 0U, 4U};
  p5_modbus_adu_view_t request = make_request(
      4U, P5_MODBUS_FUNCTION_READ_HOLDING_REGISTERS, read, sizeof(read));
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  p5_modbus_adu_view_t response;
  CHECK(decode_response(&fixture, &response));
  CHECK(response.function == P5_MODBUS_FUNCTION_READ_HOLDING_REGISTERS);
  CHECK(response.data_length == 9U);
  CHECK(response.data[0] == 8U);
  const uint8_t expected[] = {0U, 4U, 0U, 4U, 0U, 1U, 0U, 0U};
  CHECK(memcmp(&response.data[1], expected, sizeof(expected)) == 0);

  request.function = 0x10U;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(response_is_exception(
      &fixture, 0x10U, P5_MODBUS_EXCEPTION_ILLEGAL_FUNCTION));

  request.function = P5_MODBUS_FUNCTION_READ_HOLDING_REGISTERS;
  request.data_length = 3U;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(response_is_exception(
      &fixture,
      P5_MODBUS_FUNCTION_READ_HOLDING_REGISTERS,
      P5_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE));

  request.data_length = sizeof(read);
  read[3] = 0U;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(response_is_exception(
      &fixture,
      P5_MODBUS_FUNCTION_READ_HOLDING_REGISTERS,
      P5_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE));

  read[2] = 0U;
  read[3] = 5U;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(response_is_exception(
      &fixture,
      P5_MODBUS_FUNCTION_READ_HOLDING_REGISTERS,
      P5_MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS));

  read[2] = 0U;
  read[3] = 126U;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(response_is_exception(
      &fixture,
      P5_MODBUS_FUNCTION_READ_HOLDING_REGISTERS,
      P5_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE));
  return true;
}

static bool test_input_image(void)
{
  fixture_t fixture;
  CHECK(fixture_initialize(&fixture));
  uint8_t read[] = {0U, 0U, 0U, 122U};
  p5_modbus_adu_view_t request = make_request(
      4U, P5_MODBUS_FUNCTION_READ_INPUT_REGISTERS, read, sizeof(read));
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(fixture.tx.length == P5_MODBUS_SERVER_MAX_RESPONSE_SIZE);
  p5_modbus_adu_view_t response;
  CHECK(decode_response(&fixture, &response));
  CHECK(response.data_length == 245U);
  CHECK(response.data[0] == 244U);
  CHECK(response.data[1] == 0x10U);
  CHECK(response.data[2] == 0x00U);
  CHECK(response.data[243] == 0x10U);
  CHECK(response.data[244] == 0x79U);
  CHECK(fixture.input.calls == 1U);

  read[0] = 0U;
  read[1] = 121U;
  read[2] = 0U;
  read[3] = 2U;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(response_is_exception(
      &fixture,
      P5_MODBUS_FUNCTION_READ_INPUT_REGISTERS,
      P5_MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS));

  read[0] = 0U;
  read[1] = 0U;
  read[2] = 0U;
  read[3] = 1U;
  fixture.input.available = false;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(response_is_exception(
      &fixture,
      P5_MODBUS_FUNCTION_READ_INPUT_REGISTERS,
      P5_MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE));
  return true;
}

static bool test_write_commit_and_cancel(void)
{
  fixture_t fixture;
  CHECK(fixture_initialize(&fixture));
  uint8_t write[] = {0U, 0U, 0U, 4U};
  p5_modbus_adu_view_t request = make_request(
      4U, P5_MODBUS_FUNCTION_WRITE_SINGLE_REGISTER, write, sizeof(write));
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(!p5_modbus_server_has_pending_write(&fixture.server));
  CHECK(p5_modbus_server_configuration_generation(&fixture.server) == 0U);

  write[3] = 7U;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(p5_modbus_server_has_pending_write(&fixture.server));
  CHECK(p5_modbus_server_active_address(&fixture.server) == 4U);

  write[3] = 8U;
  const uint32_t calls_before_busy = fixture.tx.calls;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_REJECTED);
  CHECK(fixture.tx.calls == calls_before_busy);
  p5_modbus_server_on_tx_complete(&fixture.server);
  CHECK(!p5_modbus_server_has_pending_write(&fixture.server));
  CHECK(p5_modbus_server_active_address(&fixture.server) == 7U);
  CHECK(p5_modbus_server_configuration_generation(&fixture.server) == 1U);

  const uint8_t read[] = {0U, 0U, 0U, 1U};
  request = make_request(
      4U, P5_MODBUS_FUNCTION_READ_HOLDING_REGISTERS, read, sizeof(read));
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_IGNORED);
  request.address = 7U;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);

  write[3] = 9U;
  request = make_request(
      7U, P5_MODBUS_FUNCTION_WRITE_SINGLE_REGISTER, write, sizeof(write));
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  p5_modbus_server_on_link_failure(&fixture.server);
  CHECK(!p5_modbus_server_has_pending_write(&fixture.server));
  CHECK(p5_modbus_server_active_address(&fixture.server) == 7U);
  CHECK(p5_modbus_server_configuration_generation(&fixture.server) == 1U);

  write[0] = 0U;
  write[1] = 1U;
  write[2] = 0U;
  write[3] = 8U;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(response_is_exception(
      &fixture,
      P5_MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
      P5_MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS));

  write[0] = 0U;
  write[1] = 0U;
  write[2] = 0U;
  write[3] = 248U;
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED);
  CHECK(response_is_exception(
      &fixture,
      P5_MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
      P5_MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE));
  return true;
}

static bool test_tx_rejection(void)
{
  fixture_t fixture;
  CHECK(fixture_initialize(&fixture));
  fixture.tx.accept = false;
  const uint8_t write[] = {0U, 0U, 0U, 9U};
  const p5_modbus_adu_view_t request = make_request(
      4U, P5_MODBUS_FUNCTION_WRITE_SINGLE_REGISTER, write, sizeof(write));
  CHECK(p5_modbus_server_process(&fixture.server, &request) ==
        P5_MODBUS_SERVER_PROCESS_RESPONSE_REJECTED);
  CHECK(!p5_modbus_server_has_pending_write(&fixture.server));
  CHECK(p5_modbus_server_active_address(&fixture.server) == 4U);
  CHECK(p5_modbus_server_configuration_generation(&fixture.server) == 0U);
  const p5_modbus_server_diagnostics_t diagnostics =
      p5_modbus_server_diagnostics(&fixture.server);
  CHECK(diagnostics.tx_rejected == 1U);
  return true;
}

int main(void)
{
  CHECK(test_address_filter());
  CHECK(test_holding_and_exceptions());
  CHECK(test_input_image());
  CHECK(test_write_commit_and_cancel());
  CHECK(test_tx_rejection());
  (void)puts("modbus server tests passed");
  return 0;
}
