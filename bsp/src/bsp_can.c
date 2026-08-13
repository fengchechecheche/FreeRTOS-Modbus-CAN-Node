#include "bsp_can.h"

#include "can.h"

CAN_HandleTypeDef *bsp_can_handle(void)
{
  return &hcan1;
}
