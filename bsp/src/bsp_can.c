#include "bsp_can.h"

#include <string.h>

#include "can.h"

#ifndef P5_CAN_ACK_TX_DIAGNOSTIC_ENABLE
#define P5_CAN_ACK_TX_DIAGNOSTIC_ENABLE (0)
#endif

static bsp_can_irq_mailbox_t bsp_can_irq_mailbox;
static bsp_can_irq_notifier_t bsp_can_irq_notifier;

/* LEC is sampled when state interrupts fire; enabling its per-error interrupt
 * can starve tasks while an unacknowledged frame is automatically retried. */
#define BSP_CAN_NOTIFICATION_MASK                                              \
  (CAN_IT_TX_MAILBOX_EMPTY | CAN_IT_RX_FIFO0_MSG_PENDING |                     \
   CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE | CAN_IT_BUSOFF |               \
   CAN_IT_ERROR)

static void bsp_can_notify_from_isr(uint32_t accepted) {
  if (accepted == 0U) {
    return;
  }

  const bsp_can_irq_notifier_t notifier = bsp_can_irq_notifier;
  if (notifier != NULL) {
    notifier(accepted);
  } else {
    bsp_can_irq_mailbox_note_deferred(&bsp_can_irq_mailbox);
  }
}

static bool bsp_can_configure_filter_bank(uint32_t bank_index,
                                          const bsp_can_filter_bank_t *bank) {
  CAN_FilterTypeDef filter = {0};
  filter.FilterBank = bank_index;
  filter.FilterMode = CAN_FILTERMODE_IDLIST;
  filter.FilterScale = CAN_FILTERSCALE_16BIT;
  filter.FilterIdHigh = bank->entries[0];
  filter.FilterIdLow = bank->entries[1];
  filter.FilterMaskIdHigh = bank->entries[2];
  filter.FilterMaskIdLow = bank->entries[3];
  filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  filter.FilterActivation = ENABLE;
  filter.SlaveStartFilterBank = 14U;
  return HAL_CAN_ConfigFilter(&hcan1, &filter) == HAL_OK;
}

CAN_HandleTypeDef *bsp_can_handle(void) { return &hcan1; }

bool bsp_can_configure_filters(void) {
  bsp_can_filter_plan_t plan;
  bsp_can_filter_plan_build(&plan);
  for (uint32_t index = 0U; index < BSP_CAN_FILTER_BANK_COUNT; ++index) {
    if (!bsp_can_configure_filter_bank(index, &plan.banks[index])) {
      return false;
    }
  }
  return true;
}

bool bsp_can_start(void) {
#if P5_CAN_ACK_TX_DIAGNOSTIC_ENABLE
  if (!bsp_can_configure_filters()) {
    return false;
  }
  /* TX_ONCE must remain wire-bounded even when no other node acknowledges. */
  SET_BIT(hcan1.Instance->MCR, CAN_MCR_NART);
  if (HAL_CAN_Start(&hcan1) != HAL_OK) {
    return false;
  }
#else
  if (!bsp_can_configure_filters() || (HAL_CAN_Start(&hcan1) != HAL_OK)) {
    return false;
  }
#endif
  if (HAL_CAN_ActivateNotification(&hcan1, BSP_CAN_NOTIFICATION_MASK) !=
      HAL_OK) {
    (void)HAL_CAN_Stop(&hcan1);
    return false;
  }
  return true;
}

bool bsp_can_stop(void) {
  const HAL_StatusTypeDef notification_status =
      HAL_CAN_DeactivateNotification(&hcan1, BSP_CAN_NOTIFICATION_MASK);
  const HAL_StatusTypeDef stop_status = HAL_CAN_Stop(&hcan1);
  return (notification_status == HAL_OK) && (stop_status == HAL_OK);
}

bsp_can_send_result_t bsp_can_send(const p5_can_frame_t *frame) {
  if ((frame == NULL) ||
      !bsp_can_header_is_accepted(frame->standard_id, BSP_CAN_IDE_STANDARD,
                                  BSP_CAN_RTR_DATA, frame->dlc)) {
    return BSP_CAN_SEND_ERROR;
  }

  CAN_TxHeaderTypeDef header = {
      .StdId = frame->standard_id,
      .ExtId = 0U,
      .IDE = CAN_ID_STD,
      .RTR = CAN_RTR_DATA,
      .DLC = frame->dlc,
      .TransmitGlobalTime = DISABLE,
  };
  uint8_t data[P5_CAN_DLC];
  (void)memcpy(data, frame->data, sizeof(data));
  uint32_t mailbox = 0U;
  const HAL_StatusTypeDef result =
      HAL_CAN_AddTxMessage(&hcan1, &header, data, &mailbox);
  if (result == HAL_OK) {
    return BSP_CAN_SEND_OK;
  }
  if (result == HAL_BUSY) {
    return BSP_CAN_SEND_BUSY;
  }
  return BSP_CAN_SEND_ERROR;
}

uint32_t bsp_can_tx_free_level(void) {
  return HAL_CAN_GetTxMailboxesFreeLevel(&hcan1);
}

void bsp_can_irq_initialize(void) {
  const uint32_t previous_primask = __get_PRIMASK();
  __disable_irq();
  bsp_can_irq_notifier = NULL;
  bsp_can_irq_mailbox_initialize(&bsp_can_irq_mailbox);
  if (previous_primask == 0U) {
    __enable_irq();
  }
}

void bsp_can_register_irq_notifier(bsp_can_irq_notifier_t notifier) {
  const uint32_t previous_primask = __get_PRIMASK();
  __disable_irq();
  bsp_can_irq_notifier = notifier;
  if (previous_primask == 0U) {
    __enable_irq();
  }
}

bool bsp_can_take_irq_snapshot(bsp_can_irq_event_snapshot_t *snapshot) {
  const uint32_t previous_primask = __get_PRIMASK();
  __disable_irq();
  const bool available =
      bsp_can_irq_mailbox_take(&bsp_can_irq_mailbox, snapshot);
  if (previous_primask == 0U) {
    __enable_irq();
  }
  return available;
}

bool bsp_can_take_received(bsp_can_rx_frame_t *frame) {
  const uint32_t previous_primask = __get_PRIMASK();
  __disable_irq();
  const bool available =
      bsp_can_irq_mailbox_pop_rx(&bsp_can_irq_mailbox, frame);
  if (previous_primask == 0U) {
    __enable_irq();
  }
  return available;
}

bsp_can_irq_event_counters_t bsp_can_irq_counters(void) {
  const uint32_t previous_primask = __get_PRIMASK();
  __disable_irq();
  const bsp_can_irq_event_counters_t counters =
      bsp_can_irq_mailbox_counters(&bsp_can_irq_mailbox);
  if (previous_primask == 0U) {
    __enable_irq();
  }
  return counters;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  if (hcan != &hcan1) {
    return;
  }

  CAN_RxHeaderTypeDef header = {0};
  uint8_t data[P5_CAN_DLC] = {0U};
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, data) != HAL_OK) {
    bsp_can_notify_from_isr(bsp_can_irq_mailbox_publish_error(
        &bsp_can_irq_mailbox, HAL_CAN_GetError(hcan),
        (uint32_t)HAL_CAN_GetState(hcan)));
    return;
  }

  bsp_can_rx_frame_t frame = {
      .standard_id = header.StdId,
      .ide = header.IDE,
      .rtr = header.RTR,
      .dlc = header.DLC,
      .data = {0U},
  };
  (void)memcpy(frame.data, data, sizeof(frame.data));
  bsp_can_notify_from_isr(
      bsp_can_irq_mailbox_publish_rx(&bsp_can_irq_mailbox, &frame));
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan) {
  if (hcan == &hcan1) {
    bsp_can_notify_from_isr(
        bsp_can_irq_mailbox_publish_tx_complete(&bsp_can_irq_mailbox, 0U));
  }
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) {
  if (hcan == &hcan1) {
    bsp_can_notify_from_isr(
        bsp_can_irq_mailbox_publish_tx_complete(&bsp_can_irq_mailbox, 1U));
  }
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) {
  if (hcan == &hcan1) {
    bsp_can_notify_from_isr(
        bsp_can_irq_mailbox_publish_tx_complete(&bsp_can_irq_mailbox, 2U));
  }
}

void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan) {
  if (hcan == &hcan1) {
    bsp_can_notify_from_isr(
        bsp_can_irq_mailbox_publish_tx_abort(&bsp_can_irq_mailbox, 0U));
  }
}

void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan) {
  if (hcan == &hcan1) {
    bsp_can_notify_from_isr(
        bsp_can_irq_mailbox_publish_tx_abort(&bsp_can_irq_mailbox, 1U));
  }
}

void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan) {
  if (hcan == &hcan1) {
    bsp_can_notify_from_isr(
        bsp_can_irq_mailbox_publish_tx_abort(&bsp_can_irq_mailbox, 2U));
  }
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan) {
  if (hcan == &hcan1) {
    bsp_can_notify_from_isr(bsp_can_irq_mailbox_publish_error(
        &bsp_can_irq_mailbox, HAL_CAN_GetError(hcan),
        (uint32_t)HAL_CAN_GetState(hcan)));
  }
}
