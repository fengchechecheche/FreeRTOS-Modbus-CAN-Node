#ifndef APP_CAN_RUNTIME_H
#define APP_CAN_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "app_health_policy.h"
#include "app_measurement.h"
#include "p5_can_contract.h"

#define APP_CAN_EVENT_FIFO_DEPTH UINT32_C(8)
#define APP_CAN_TX_DRAIN_BUDGET UINT32_C(3)
#define APP_CAN_EVENT_BURST_LIMIT UINT32_C(2)
#define APP_CAN_RECOVERY_DELAY_MS UINT32_C(1000)
#define APP_CAN_RECOVERY_ATTEMPT_LIMIT UINT32_C(3)
#define APP_CAN_PERIODIC_FRAME_COUNT UINT32_C(6)
#define APP_CAN_DIAGNOSTIC_MIN_INTERVAL_MS UINT32_C(100)

#define APP_CAN_ERROR_WARNING UINT32_C(0x00000001)
#define APP_CAN_ERROR_PASSIVE UINT32_C(0x00000002)
#define APP_CAN_ERROR_BUS_OFF UINT32_C(0x00000004)
#define APP_CAN_ERROR_ARBITRATION_LOST UINT32_C(0x00000008)
#define APP_CAN_ERROR_ACK UINT32_C(0x00000010)
#define APP_CAN_ERROR_TX UINT32_C(0x00000020)
#define APP_CAN_ERROR_RX_OVERRUN UINT32_C(0x00000040)

typedef enum {
  APP_CAN_TELEMETRY_HEARTBEAT = 0,
  APP_CAN_TELEMETRY_HEALTH,
  APP_CAN_TELEMETRY_CLIMATE_PAIR,
  APP_CAN_TELEMETRY_ILLUMINANCE,
  APP_CAN_TELEMETRY_VIBRATION,
  APP_CAN_TELEMETRY_GROUP_COUNT
} app_can_telemetry_group_t;

typedef enum {
  APP_CAN_PERIODIC_HEARTBEAT = 0,
  APP_CAN_PERIODIC_HEALTH,
  APP_CAN_PERIODIC_CLIMATE_PRIMARY,
  APP_CAN_PERIODIC_CLIMATE_SECONDARY,
  APP_CAN_PERIODIC_ILLUMINANCE,
  APP_CAN_PERIODIC_VIBRATION
} app_can_periodic_frame_index_t;

typedef enum {
  APP_CAN_TX_TOKEN_NONE = 0,
  APP_CAN_TX_TOKEN_DIAGNOSTIC,
  APP_CAN_TX_TOKEN_EVENT,
  APP_CAN_TX_TOKEN_TELEMETRY
} app_can_tx_token_kind_t;

typedef struct {
  app_can_tx_token_kind_t kind;
  uint8_t group;
  uint8_t part;
} app_can_tx_token_t;

typedef struct {
  uint16_t event_code;
  uint8_t source;
  p5_can_frame_t frame;
} app_can_event_entry_t;

typedef struct {
  uint32_t event_enqueued;
  uint32_t event_coalesced;
  uint32_t event_dropped;
  uint32_t telemetry_published;
  uint32_t telemetry_replaced;
  uint32_t telemetry_dropped;
  uint32_t tx_peeked;
  uint32_t tx_committed;
  uint32_t hal_busy;
  uint32_t maximum_pending;
  uint32_t diagnostic_enqueued;
  uint32_t diagnostic_dropped;
  uint32_t diagnostic_sent;
} app_can_tx_counters_t;

typedef struct {
  app_can_event_entry_t events[APP_CAN_EVENT_FIFO_DEPTH];
  p5_can_frame_t telemetry[APP_CAN_TELEMETRY_GROUP_COUNT][2];
  uint8_t telemetry_pending[APP_CAN_TELEMETRY_GROUP_COUNT];
  uint8_t event_head;
  uint8_t event_count;
  uint8_t event_streak;
  uint8_t round_robin_group;
  bool diagnostic_pending;
  p5_can_frame_t diagnostic_response;
  app_can_tx_counters_t counters;
} app_can_tx_scheduler_t;

typedef enum {
  APP_CAN_DIAGNOSTIC_READY = 0,
  APP_CAN_DIAGNOSTIC_MALFORMED,
  APP_CAN_DIAGNOSTIC_DUPLICATE,
  APP_CAN_DIAGNOSTIC_RATE_LIMITED
} app_can_diagnostic_result_t;

typedef struct {
  uint32_t valid_requests;
  uint32_t malformed_requests;
  uint32_t duplicate_requests;
  uint32_t rate_limited_requests;
} app_can_diagnostic_counters_t;

typedef struct {
  bool has_last_request;
  uint8_t last_sequence;
  uint32_t last_nonce;
  uint32_t last_accepted_ms;
  app_can_diagnostic_counters_t counters;
} app_can_diagnostic_responder_t;

typedef enum {
  APP_CAN_CONTROLLER_STOPPED = 0,
  APP_CAN_CONTROLLER_STARTING,
  APP_CAN_CONTROLLER_ACTIVE,
  APP_CAN_CONTROLLER_WARNING,
  APP_CAN_CONTROLLER_PASSIVE,
  APP_CAN_CONTROLLER_BUS_OFF,
  APP_CAN_CONTROLLER_RECOVERY_WAIT,
  APP_CAN_CONTROLLER_RECOVERY_LATCHED
} app_can_controller_state_t;

typedef struct {
  uint32_t start_attempts;
  uint32_t start_successes;
  uint32_t start_failures;
  uint32_t recovery_attempts;
  uint32_t warning_transitions;
  uint32_t passive_transitions;
  uint32_t bus_off_transitions;
  uint32_t arbitration_lost;
  uint32_t ack_errors;
  uint32_t tx_errors;
  uint32_t rx_overruns;
} app_can_controller_counters_t;

typedef struct {
  app_can_controller_state_t state;
  uint32_t recovery_deadline_ms;
  uint32_t last_error_bits;
  uint8_t recovery_attempts_in_episode;
  app_can_controller_counters_t counters;
} app_can_controller_t;

typedef struct {
  app_can_controller_state_t controller_state;
  app_can_controller_counters_t controller_counters;
  app_can_tx_counters_t tx_counters;
  uint32_t pending_frames;
  uint32_t last_hal_error;
  uint8_t recovery_attempts_in_episode;
  uint32_t rx_accepted;
  uint32_t rx_dropped;
  uint32_t rx_invalid;
  uint32_t tx_completed;
  uint32_t tx_aborted;
  uint32_t deferred_notifications;
  bool hardware_started;
} app_can_runtime_snapshot_t;

void app_can_tx_scheduler_initialize(app_can_tx_scheduler_t *scheduler);
bool app_can_tx_enqueue_event(app_can_tx_scheduler_t *scheduler,
                              uint16_t event_code, uint8_t source,
                              const p5_can_frame_t *frame);
bool app_can_tx_publish_telemetry(app_can_tx_scheduler_t *scheduler,
                                  app_can_telemetry_group_t group,
                                  const p5_can_frame_t *frame);
bool app_can_tx_publish_climate_pair(app_can_tx_scheduler_t *scheduler,
                                     const p5_can_frame_t *primary,
                                     const p5_can_frame_t *secondary);
bool app_can_tx_publish_diagnostic_response(
    app_can_tx_scheduler_t *scheduler,
    const p5_can_frame_t *frame);
bool app_can_tx_peek(app_can_tx_scheduler_t *scheduler, p5_can_frame_t *frame,
                     app_can_tx_token_t *token);
bool app_can_tx_commit(app_can_tx_scheduler_t *scheduler,
                       app_can_tx_token_t token);
void app_can_tx_note_hal_busy(app_can_tx_scheduler_t *scheduler);
uint32_t app_can_tx_pending(const app_can_tx_scheduler_t *scheduler);
app_can_tx_counters_t
app_can_tx_counters(const app_can_tx_scheduler_t *scheduler);
void app_can_diagnostic_initialize(app_can_diagnostic_responder_t *responder);
app_can_diagnostic_result_t app_can_diagnostic_process(
    app_can_diagnostic_responder_t *responder,
    const p5_can_frame_t *request,
    uint32_t now_ms,
    p5_can_frame_t *response);
app_can_diagnostic_counters_t app_can_diagnostic_counters(
    const app_can_diagnostic_responder_t *responder);
bool app_can_build_periodic_frames(
    const app_measurement_snapshot_t *measurement,
    const app_health_decision_t *health, uint32_t image_generation,
    uint32_t uptime_s, uint8_t sequence,
    p5_can_frame_t frames[APP_CAN_PERIODIC_FRAME_COUNT]);

void app_can_controller_initialize(app_can_controller_t *controller);
bool app_can_controller_begin_initial_start(app_can_controller_t *controller);
bool app_can_controller_begin_recovery(app_can_controller_t *controller,
                                       uint32_t now_ms);
bool app_can_controller_complete_start(app_can_controller_t *controller,
                                       bool success, uint32_t now_ms);
bool app_can_controller_on_error(app_can_controller_t *controller,
                                 uint32_t error_bits, uint32_t now_ms);
bool app_can_controller_recovery_due(const app_can_controller_t *controller,
                                     uint32_t now_ms);

#endif
