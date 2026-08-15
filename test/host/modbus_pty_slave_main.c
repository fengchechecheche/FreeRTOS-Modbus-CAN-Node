#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "app_modbus_register_image.h"
#include "p5_modbus_rtu_stream.h"
#include "p5_modbus_rtu_timing.h"
#include "p5_modbus_server.h"

enum {
  P5_PTY_BAUD_RATE = 19200,
  P5_PTY_POLL_TIMEOUT_MS = 1,
  P5_PTY_EXPECTED_ACCEPTED_FRAMES = 9,
  P5_PTY_EXPECTED_ADDRESSED_REQUESTS = 7,
  P5_PTY_EXPECTED_NORMAL_RESPONSES = 3,
  P5_PTY_EXPECTED_EXCEPTION_RESPONSES = 4,
  P5_PTY_EXPECTED_TX_CALLS = 7
};

typedef struct {
  int fd;
  p5_modbus_rtu_timing_t timing;
  p5_modbus_rtu_stream_t stream;
  p5_modbus_server_t server;
  app_modbus_register_source_t source;
  uint8_t response[P5_MODBUS_RTU_MAX_ADU_SIZE];
  uint16_t input_registers[P5_MODBUS_SERVER_INPUT_REGISTER_COUNT];
  uint32_t tx_calls;
  size_t maximum_tx_length;
} p5_pty_harness_t;

static volatile sig_atomic_t p5_pty_running = 1;

static void p5_pty_stop(int signal_number) {
  (void)signal_number;
  p5_pty_running = 0;
}

static bool p5_pty_monotonic_us(uint32_t *ticks_out) {
  struct timespec value;
  if ((ticks_out == NULL) || (clock_gettime(CLOCK_MONOTONIC, &value) != 0)) {
    return false;
  }

  const uint64_t microseconds = ((uint64_t)value.tv_sec * UINT64_C(1000000)) +
                                ((uint64_t)value.tv_nsec / UINT64_C(1000));
  *ticks_out = (uint32_t)microseconds;
  return true;
}

static bool p5_pty_write_all(int fd, const uint8_t *data, size_t length) {
  size_t offset = 0U;
  while (offset < length) {
    const ssize_t written = write(fd, &data[offset], length - offset);
    if (written > 0) {
      offset += (size_t)written;
      continue;
    }
    if ((written < 0) && (errno == EINTR)) {
      continue;
    }
    return false;
  }
  return true;
}

static bool p5_pty_provide_input(void *context, uint16_t *registers,
                                 size_t register_count) {
  const app_modbus_register_source_t *source = context;
  return app_modbus_register_image_build(source, registers, register_count);
}

static bool p5_pty_transmit(void *context, const uint8_t *frame,
                            size_t frame_length) {
  p5_pty_harness_t *harness = context;
  ++harness->tx_calls;
  if (frame_length > harness->maximum_tx_length) {
    harness->maximum_tx_length = frame_length;
  }
  return p5_pty_write_all(harness->fd, frame, frame_length);
}

static void p5_pty_consume(void *context, const uint8_t *frame,
                           size_t frame_length,
                           const p5_modbus_adu_view_t *view) {
  p5_pty_harness_t *harness = context;
  (void)frame;
  (void)frame_length;
  const p5_modbus_server_process_result_t result =
      p5_modbus_server_process(&harness->server, view);
  if (result == P5_MODBUS_SERVER_PROCESS_RESPONSE_ACCEPTED) {
    p5_modbus_server_on_tx_complete(&harness->server);
  } else if (result == P5_MODBUS_SERVER_PROCESS_RESPONSE_REJECTED) {
    p5_modbus_server_on_link_failure(&harness->server);
  }
}

static void p5_pty_initialize_source(app_modbus_register_source_t *source) {
  (void)memset(source, 0, sizeof(*source));
  source->register_image_generation = 1U;
  source->measurement.schema_revision = APP_MEASUREMENT_SCHEMA_REVISION;
  source->measurement.evaluated_monotonic_ms = 1000U;
  source->sensor_monitor_schema_revision = APP_SENSOR_MONITOR_SCHEMA_REVISION;
  source->health_state = APP_HEALTH_BOOTSTRAP;

  source->measurement.bme280.metadata.source = APP_MEASUREMENT_SOURCE_BME280;
  source->measurement.veml7700.metadata.source =
      APP_MEASUREMENT_SOURCE_VEML7700;
  source->measurement.adxl345_sample.metadata.source =
      APP_MEASUREMENT_SOURCE_ADXL345_SAMPLE;
  source->measurement.adxl345_feature.metadata.source =
      APP_MEASUREMENT_SOURCE_ADXL345_FEATURE;
}

static bool p5_pty_initialize(p5_pty_harness_t *harness, int fd) {
  (void)memset(harness, 0, sizeof(*harness));
  harness->fd = fd;
  p5_pty_initialize_source(&harness->source);

  if (!p5_modbus_rtu_timing_8e1(P5_PTY_BAUD_RATE, &harness->timing)) {
    return false;
  }

  const p5_modbus_server_config_t server_config = {
      .input_context = &harness->source,
      .input_provider = p5_pty_provide_input,
      .tx_context = harness,
      .tx_sink = p5_pty_transmit,
      .response_buffer = harness->response,
      .response_capacity = sizeof(harness->response),
      .input_registers = harness->input_registers,
      .input_register_capacity = P5_MODBUS_SERVER_INPUT_REGISTER_COUNT,
  };
  if (!p5_modbus_server_initialize(&harness->server, &server_config)) {
    return false;
  }

  const p5_modbus_rtu_stream_config_t stream_config = {
      .character_ticks = harness->timing.character_us,
      .t1_5_ticks = harness->timing.inter_character_us,
      .t3_5_ticks = harness->timing.inter_frame_us,
      .consumer = p5_pty_consume,
      .consumer_context = harness,
  };
  return p5_modbus_rtu_stream_initialize(&harness->stream, &stream_config);
}

static bool p5_pty_parse_fd(const char *text, int *fd_out) {
  if ((text == NULL) || (fd_out == NULL)) {
    return false;
  }
  errno = 0;
  char *end = NULL;
  const long value = strtol(text, &end, 10);
  if ((errno != 0) || (end == text) || (*end != '\0') || (value < 0L) ||
      (value > INT32_MAX)) {
    return false;
  }
  *fd_out = (int)value;
  return true;
}

static bool p5_pty_run(p5_pty_harness_t *harness) {
  struct pollfd descriptor = {
      .fd = harness->fd,
      .events = POLLIN,
      .revents = 0,
  };
  uint8_t bytes[P5_MODBUS_RTU_MAX_ADU_SIZE];

  while (p5_pty_running != 0) {
    descriptor.revents = 0;
    const int result = poll(&descriptor, 1U, P5_PTY_POLL_TIMEOUT_MS);
    if (result < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }

    uint32_t now = 0U;
    if (!p5_pty_monotonic_us(&now)) {
      return false;
    }
    p5_modbus_rtu_stream_poll(&harness->stream, now);

    if (result == 0) {
      continue;
    }
    if ((descriptor.revents & (POLLERR | POLLNVAL)) != 0) {
      return false;
    }
    if ((descriptor.revents & POLLIN) == 0) {
      continue;
    }

    const ssize_t length = read(harness->fd, bytes, sizeof(bytes));
    if (length < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (length == 0) {
      return false;
    }

    uint32_t byte_end = now;
    for (ssize_t index = 0; index < length; ++index) {
      p5_modbus_rtu_stream_push_byte(&harness->stream, bytes[index], byte_end);
      byte_end += harness->timing.character_us;
    }
  }
  return true;
}

static bool p5_pty_expected_result(const p5_pty_harness_t *harness) {
  const p5_modbus_rtu_stream_counters_t stream =
      p5_modbus_rtu_stream_counters(&harness->stream);
  const p5_modbus_server_diagnostics_t server =
      p5_modbus_server_diagnostics(&harness->server);
  const uint32_t exceptions = server.illegal_function_exceptions +
                              server.illegal_data_address_exceptions +
                              server.illegal_data_value_exceptions +
                              server.server_device_failure_exceptions;

  return (stream.accepted_frames == P5_PTY_EXPECTED_ACCEPTED_FRAMES) &&
         (stream.crc_mismatches == 1U) &&
         (server.addressed_requests == P5_PTY_EXPECTED_ADDRESSED_REQUESTS) &&
         (server.normal_responses == P5_PTY_EXPECTED_NORMAL_RESPONSES) &&
         (exceptions == P5_PTY_EXPECTED_EXCEPTION_RESPONSES) &&
         (server.broadcast_ignored == 1U) && (server.foreign_ignored == 1U) &&
         (harness->tx_calls == P5_PTY_EXPECTED_TX_CALLS) &&
         (harness->maximum_tx_length == P5_MODBUS_SERVER_MAX_RESPONSE_SIZE);
}

static void p5_pty_print_summary(const p5_pty_harness_t *harness, bool passed) {
  const p5_modbus_rtu_stream_counters_t stream =
      p5_modbus_rtu_stream_counters(&harness->stream);
  const p5_modbus_server_diagnostics_t server =
      p5_modbus_server_diagnostics(&harness->server);
  (void)printf("P5 MODBUS PTY SLAVE: %s frames=%u crc_mismatches=%u "
               "addressed=%u normal=%u tx=%u max_tx=%zu\n",
               passed ? "PASS" : "FAIL", (unsigned int)stream.accepted_frames,
               (unsigned int)stream.crc_mismatches,
               (unsigned int)server.addressed_requests,
               (unsigned int)server.normal_responses,
               (unsigned int)harness->tx_calls, harness->maximum_tx_length);
}

int main(int argc, char **argv) {
  int fd = -1;
  if ((argc != 2) || !p5_pty_parse_fd(argv[1], &fd)) {
    (void)fprintf(stderr, "usage: %s PTY_MASTER_FD\n", argv[0]);
    return EXIT_FAILURE;
  }

  p5_pty_harness_t harness;
  if (!p5_pty_initialize(&harness, fd)) {
    (void)fprintf(stderr, "P5 MODBUS PTY SLAVE: initialization failed\n");
    return EXIT_FAILURE;
  }
  if ((signal(SIGTERM, p5_pty_stop) == SIG_ERR) ||
      (signal(SIGINT, p5_pty_stop) == SIG_ERR)) {
    (void)fprintf(stderr, "P5 MODBUS PTY SLAVE: signal setup failed\n");
    return EXIT_FAILURE;
  }

  (void)printf("P5 MODBUS PTY SLAVE: READY\n");
  (void)fflush(stdout);
  const bool run_ok = p5_pty_run(&harness);
  const bool passed = run_ok && p5_pty_expected_result(&harness);
  p5_pty_print_summary(&harness, passed);
  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
