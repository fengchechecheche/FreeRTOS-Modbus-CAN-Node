#!/usr/bin/env python3
"""Check stable P5 BSP candidate facts without claiming hardware validation."""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Mapping


CheckTable = dict[Path, list[tuple[str, str]]]


CHECKS: CheckTable = {
    Path("freertos_modbus_can_node.ioc"): [
        ("board", "BSP_IP_NAME=NUCLEO-F446RE"),
        ("mcu", "Mcu.UserName=STM32F446RETx"),
        ("swdio", "PA13.Signal=SYS_JTMS-SWDIO"),
        ("swclk", "PA14.Signal=SYS_JTCK-SWCLK"),
        ("usart2 tx", "PA2.Signal=USART2_TX"),
        ("usart2 rx", "PA3.Signal=USART2_RX"),
        ("spi1 sck", "PA5.Signal=SPI1_SCK"),
        ("spi1 miso", "PA6.Signal=SPI1_MISO"),
        ("spi1 mosi", "PA7.Signal=SPI1_MOSI"),
        ("rs485 de label", "PA8.GPIO_Label=RS485_DE"),
        ("rs485 de low", "PA8.PinState=GPIO_PIN_RESET"),
        ("usart1 tx", "PA9.Signal=USART1_TX"),
        ("usart1 rx", "PA10.Signal=USART1_RX"),
        ("i2c2 scl", "PB10.Signal=I2C2_SCL"),
        ("i2c2 sda", "PB3.Signal=I2C2_SDA"),
        ("adxl int label", "PB4.GPIO_Label=ADXL345_INT1"),
        ("bme cs label", "PB6.GPIO_Label=BME280_CS"),
        ("bme cs high", "PB6.PinState=GPIO_PIN_SET"),
        ("can1 rx", "PB8.Signal=CAN1_RX"),
        ("can1 tx", "PB9.Signal=CAN1_TX"),
        ("adxl cs label", "PC7.GPIO_Label=ADXL345_CS"),
        ("adxl cs high", "PC7.PinState=GPIO_PIN_SET"),
        ("hclk", "RCC.HCLKFreq_Value=180000000"),
        ("pclk1", "RCC.APB1Freq_Value=45000000"),
        ("pclk2", "RCC.APB2Freq_Value=90000000"),
        ("sysclk", "RCC.SYSCLKFreq_VALUE=180000000"),
        ("pll m", "RCC.PLLM=8"),
        ("pll n", "RCC.PLLN=180"),
        ("usart1 baud", "USART1.BaudRate=19200"),
        ("usart1 parity", "USART1.Parity=PARITY_EVEN"),
        ("usart1 word length", "USART1.WordLength=WORDLENGTH_9B"),
        ("usart1 irq", "NVIC.USART1_IRQn=true"),
        ("usart1 rtos irq priority", "NVIC.USART1_IRQn=true\\:6\\:0"),
        ("usart1 rx rtos irq priority", "NVIC.DMA2_Stream2_IRQn=true\\:6\\:0"),
        ("usart1 tx rtos irq priority", "NVIC.DMA2_Stream7_IRQn=true\\:6\\:0"),
        ("adxl exti rtos irq priority", "NVIC.EXTI4_IRQn=true\\:6\\:0"),
        ("adxl int pull down", "PB4.GPIO_PuPd=GPIO_PULLDOWN"),
        ("usart1 rx dma", "Dma.USART1_RX.0.Instance=DMA2_Stream2"),
        ("usart1 rx dma normal", "Dma.USART1_RX.0.Mode=DMA_NORMAL"),
        ("usart1 tx dma", "Dma.USART1_TX.1.Instance=DMA2_Stream7"),
        ("usart1 tx dma normal", "Dma.USART1_TX.1.Mode=DMA_NORMAL"),
        ("spi1 mode", "SPI1.Mode=SPI_MODE_MASTER"),
        ("spi1 polarity", "SPI1.CLKPolarity=SPI_POLARITY_HIGH"),
        ("spi1 phase", "SPI1.CLKPhase=SPI_PHASE_2EDGE"),
        ("spi1 prescaler", "SPI1.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_32"),
        ("can1 baud", "CAN1.CalculateBaudRate=500000"),
        ("can1 prescaler", "CAN1.Prescaler=6"),
        ("nvic priority group", "NVIC.PriorityGroup=NVIC_PRIORITYGROUP_4"),
        ("hal timebase irq", "NVIC.TimeBase=TIM6_DAC_IRQn"),
        ("hal timebase ip", "NVIC.TimeBaseIP=TIM6"),
        ("tim6 virtual mode", "VP_SYS_VS_tim6.Mode=TIM6"),
        ("iwdg prescaler", "IWDG.Prescaler=IWDG_PRESCALER_256"),
        ("iwdg reload", "IWDG.Reload=999"),
    ],
    Path("Core/Inc/stm32f4xx_hal_conf.h"): [
        ("tim hal enabled", "#define HAL_TIM_MODULE_ENABLED"),
        ("iwdg hal enabled", "#define HAL_IWDG_MODULE_ENABLED"),
    ],
    Path("Core/Src/stm32f4xx_hal_msp.c"): [
        ("nvic priority group", "HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);"),
    ],
    Path("Core/Inc/main.h"): [
        ("adxl cs pin", "#define ADXL345_CS_Pin GPIO_PIN_7"),
        ("adxl cs port", "#define ADXL345_CS_GPIO_Port GPIOC"),
        ("rs485 de pin", "#define RS485_DE_Pin GPIO_PIN_8"),
        ("rs485 de port", "#define RS485_DE_GPIO_Port GPIOA"),
        ("adxl int pin", "#define ADXL345_INT1_Pin GPIO_PIN_4"),
        ("adxl int port", "#define ADXL345_INT1_GPIO_Port GPIOB"),
        ("bme cs pin", "#define BME280_CS_Pin GPIO_PIN_6"),
        ("bme cs port", "#define BME280_CS_GPIO_Port GPIOB"),
    ],
    Path("Core/Src/main.c"): [
        ("hsi source", "RCC_OscInitStruct.HSIState = RCC_HSI_ON;"),
        ("pll source", "RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;"),
        ("pll m generated", "RCC_OscInitStruct.PLL.PLLM = 8;"),
        ("pll n generated", "RCC_OscInitStruct.PLL.PLLN = 180;"),
        ("pll p generated", "RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;"),
        ("gpio init", "MX_GPIO_Init();"),
        ("dma init", "MX_DMA_Init();"),
        ("can init", "MX_CAN1_Init();"),
        ("i2c init", "MX_I2C2_Init();"),
        ("spi init", "MX_SPI1_Init();"),
        ("usart1 init", "MX_USART1_UART_Init();"),
        ("usart2 init", "MX_USART2_UART_Init();"),
        ("lsi enabled", "RCC_OscInitStruct.LSIState = RCC_LSI_ON;"),
        ("iwdg init", "MX_IWDG_Init();"),
        ("app init", "app_boot_initialize()"),
    ],
    Path("Core/Src/gpio.c"): [
        ("adxl cs safe", "HAL_GPIO_WritePin(ADXL345_CS_GPIO_Port, ADXL345_CS_Pin, GPIO_PIN_SET);"),
        ("rs485 de safe", "HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET);"),
        ("bme cs safe", "HAL_GPIO_WritePin(BME280_CS_GPIO_Port, BME280_CS_Pin, GPIO_PIN_SET);"),
        ("adxl int pull down", "GPIO_InitStruct.Pull = GPIO_PULLDOWN;"),
        ("adxl exti priority", "HAL_NVIC_SetPriority(EXTI4_IRQn, 6, 0);"),
        ("adxl exti enabled", "HAL_NVIC_EnableIRQ(EXTI4_IRQn);"),
    ],
    Path("Core/Src/usart.c"): [
        ("usart1 baud generated", "huart1.Init.BaudRate = 19200;"),
        ("usart1 9b", "huart1.Init.WordLength = UART_WORDLENGTH_9B;"),
        ("usart1 parity generated", "huart1.Init.Parity = UART_PARITY_EVEN;"),
        ("usart2 baud generated", "huart2.Init.BaudRate = 115200;"),
        ("usart2 8b", "huart2.Init.WordLength = UART_WORDLENGTH_8B;"),
        ("usart2 parity generated", "huart2.Init.Parity = UART_PARITY_NONE;"),
        ("rx dma generated", "hdma_usart1_rx.Instance = DMA2_Stream2;"),
        ("rx dma channel", "hdma_usart1_rx.Init.Channel = DMA_CHANNEL_4;"),
        ("tx dma generated", "hdma_usart1_tx.Instance = DMA2_Stream7;"),
        ("tx dma channel", "hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;"),
        ("usart1 irq generated", "HAL_NVIC_EnableIRQ(USART1_IRQn);"),
        ("usart1 irq priority", "HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);"),
    ],
    Path("Core/Src/dma.c"): [
        ("usart1 rx dma irq priority", "HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 6, 0);"),
        ("usart1 tx dma irq priority", "HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 6, 0);"),
    ],
    Path("Core/Src/spi.c"): [
        ("spi mode generated", "hspi1.Init.Mode = SPI_MODE_MASTER;"),
        ("spi polarity generated", "hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;"),
        ("spi phase generated", "hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;"),
        ("spi software nss", "hspi1.Init.NSS = SPI_NSS_SOFT;"),
        ("spi msb", "hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;"),
    ],
    Path("Core/Src/i2c.c"): [
        ("i2c speed", "hi2c2.Init.ClockSpeed = 100000;"),
        ("i2c 7bit", "hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;"),
    ],
    Path("Core/Src/can.c"): [
        ("can prescaler generated", "hcan1.Init.Prescaler = 6;"),
        ("can bs1", "hcan1.Init.TimeSeg1 = CAN_BS1_12TQ;"),
        ("can bs2", "hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;"),
    ],
    Path("Core/Src/stm32f4xx_it.c"): [
        ("adxl exti handler", "void EXTI4_IRQHandler(void)"),
        ("adxl exti hal dispatch", "HAL_GPIO_EXTI_IRQHandler(ADXL345_INT1_Pin);"),
        ("usart1 handler", "void USART1_IRQHandler(void)"),
        ("rx dma handler", "void DMA2_Stream2_IRQHandler(void)"),
        ("tx dma handler", "void DMA2_Stream7_IRQHandler(void)"),
        ("tim6 irq handler", "void TIM6_DAC_IRQHandler(void)"),
        ("tim6 hal dispatch", "HAL_TIM_IRQHandler(&htim6);"),
    ],
    Path("Core/Src/stm32f4xx_hal_timebase_tim.c"): [
        ("tim6 hal init tick", "HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)"),
        ("tim6 instance", "htim6.Instance = TIM6;"),
        ("tim6 start", "HAL_TIM_Base_Start_IT(&htim6)"),
    ],
    Path("cmake/firmware.cmake"): [
        ("tim6 timebase source", "Core/Src/stm32f4xx_hal_timebase_tim.c"),
        ("tim hal source", "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c"),
        ("tim ex hal source", "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c"),
        ("freertos tasks source", "Middlewares/Third_Party/FreeRTOS/Source/tasks.c"),
        ("cm4f port source", "Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c"),
        ("rs485 irq event source", "bsp/src/bsp_rs485_irq_event.c"),
        ("modbus transport source", "app/src/app_modbus_transport.c"),
        ("modbus register image source", "app/src/app_modbus_register_image.c"),
        ("modbus server source", "protocol/src/p5_modbus_server.c"),
        ("modbus stream source", "protocol/src/p5_modbus_rtu_stream.c"),
        ("transport policy source", "app/src/app_transport_policy.c"),
        ("health policy source", "app/src/app_health_policy.c"),
        ("reset reason source", "app/src/app_reset_reason.c"),
        ("generated iwdg source", "Core/Src/iwdg.c"),
        ("iwdg hal source", "Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_iwdg.c"),
        ("watchdog bsp source", "bsp/src/bsp_watchdog.c"),
        ("adxl driver source", "sensors/src/adxl345.c"),
        ("adxl adapter source", "app/src/app_adxl345.c"),
        ("adxl irq source", "bsp/src/bsp_adxl345_irq.c"),
        ("sensor monitor source", "app/src/app_sensor_monitor.c"),
        (
            "adxl hil diagnostic compile definition",
            "P5_ADXL345_HIL_DIAGNOSTIC_ENABLE=1",
        ),
    ],
    Path("bsp/include/bsp_clock.h"): [
        ("clock expected sysclk", "BSP_CLOCK_EXPECTED_SYSCLK_HZ UINT32_C(180000000)"),
        ("clock tick api", "bsp_clock_tick_ms(void)"),
        ("cycle counter initialize", "bsp_clock_cycle_counter_initialize(void)"),
        ("cycle counter now", "bsp_clock_cycle_now(void)"),
        ("cycle scale", "bsp_clock_cycles_per_us(void)"),
        ("clock profile api", "bsp_clock_get_profile(void)"),
        ("clock elapsed api", "bsp_clock_elapsed_ms"),
    ],
    Path("bsp/include/bsp_rs485.h"): [
        ("rs485 init api", "bsp_rs485_initialize(void)"),
        ("rs485 send api", "bsp_rs485_send"),
        ("rs485 receive api", "bsp_rs485_take_received"),
        ("rs485 chunk receive api", "bsp_rs485_take_received_chunk"),
        ("rs485 diagnostics api", "bsp_rs485_get_diagnostics(void)"),
    ],
    Path("bsp/include/bsp_spi_bus.h"): [
        ("spi timeout", "BSP_SPI_BUS_DEFAULT_TIMEOUT_MS UINT32_C(20)"),
        ("spi register api", "bsp_spi_bus_read_register"),
    ],
    Path("bsp/include/bsp_i2c_bus.h"): [
        ("i2c timeout", "BSP_I2C_BUS_DEFAULT_TIMEOUT_MS UINT32_C(20)"),
        ("i2c ready api", "bsp_i2c_bus_is_device_ready"),
        ("i2c register api", "bsp_i2c_bus_read_register"),
    ],
    Path("bsp/include/bsp_can.h"): [
        ("can handle api", "bsp_can_handle(void)"),
    ],
    Path("CMakeLists.txt"): [
        (
            "rs485 smoke default off",
            'option(P5_RS485_LOOPBACK_SMOKE "Enable the bounded PA9/PA10 loopback smoke" OFF)',
        ),
        (
            "device probe smoke default off",
            'option(P5_DEVICE_PROBE_SMOKE "Enable the one-shot SPI/I2C device probe" OFF)',
        ),
        (
            "irq notification smoke default off",
            'option(P5_IRQ_NOTIFICATION_SMOKE "Enable bounded DWT IRQ latency summaries" OFF)',
        ),
        (
            "iwdg reset smoke default off",
            'option(P5_IWDG_RESET_SMOKE "Enable one controlled IWDG reset smoke" OFF)',
        ),
        (
            "adxl hil diagnostic default off",
            'option(P5_ADXL345_HIL_DIAGNOSTIC "Enable bounded ADXL345 HIL diagnostic output" OFF)',
        ),
        ("ownership host test", "add_test(NAME p5.host.ownership"),
        ("health host test", "add_test(NAME p5.host.health"),
        ("bme280 host test", "add_test(NAME p5.host.bme280"),
        ("veml7700 host test", "add_test(NAME p5.host.veml7700"),
        ("adxl345 host test", "add_test(NAME p5.host.adxl345"),
        ("modbus stream host test", "add_test(NAME p5.host.modbus_stream"),
        ("modbus server host test", "add_test(NAME p5.host.modbus_server"),
        ("modbus register image host test", "add_test(NAME p5.host.modbus_register_image"),
        ("sensor matrix host test", "add_test(NAME p5.host.sensor_matrix"),
    ],
    Path("docs/bsp_contract.md"): [
        ("candidate boundary", "BSP_CONTRACT_CANDIDATE_FROZEN"),
        ("hardware waiting", "WAITING_FOR_HARDWARE"),
        ("hardware upgrade boundary", "BSP_CONTRACT_HARDWARE_FROZEN"),
        ("hal tick owner", "HAL tick source = TIM6"),
        ("rtos tick reservation", "保留给后续原生 FreeRTOS kernel tick"),
        ("rtos handler owner", "接管 SysTick、PendSV 与 SVC"),
        ("spi i2c runtime owner", "`acquisition_task` 是 SPI1 与 I²C2 的唯一运行期 owner"),
        ("busy flag not mutex", "不提供跨 task 同步"),
    ],
    Path("docs/queue_ownership_report.md"): [
        ("event linked boundary", "`LINKED_NOT_WORKLOAD_EXECUTED`"),
        ("runtime stress not run", "Runtime queue/mutex stress: `NOT_RUN`"),
        ("hardware waiting", "Hardware status: `WAITING_FOR_HARDWARE`"),
        ("measurement candidate implemented", "system and S4 measurement candidate implemented"),
        ("address write local", "active-address write remains protocol-task local"),
    ],
    Path("docs/health_recovery_report.md"): [
        (
            "s3 bounded hardware gate",
            "S3 bounded hardware gate: `PASS_WDG_01`",
        ),
        ("iwdg bounded hardware pass", "IWDG runtime: `PASS_HARDWARE_BOUNDED`"),
        (
            "reset-only persistence boundary",
            "Reset-record persistence: `PASS_RESET_ONLY / NOT_CLAIMED_POWER_LOSS`",
        ),
        ("health partial hardware pass", "Hardware status: `PARTIAL_PASS`"),
        ("single feed owner", "the only owner of the watchdog feed decision"),
    ],
    Path("docs/s4_validation.md"): [
        (
            "s4 software review boundary",
            "Software status: `READY_FOR_CONTENT_REVIEW + READY_FOR_HARDWARE`",
        ),
        (
            "s5 software planning handoff",
            "S4 software handoff: `READY_FOR_S5_SOFTWARE_PLAN`",
        ),
        ("physical fault injection not run", "Physical sensor fault injection: `NOT_RUN`"),
        ("physical 60-minute run not run", "60-minute physical run: `NOT_RUN`"),
        ("virtual matrix not physical", "not evidence of board wiring"),
        ("local fault feed allowed", "feed remains allowed"),
        ("protocol remains absent", "Modbus registers and CAN"),
    ],
    Path("config/FreeRTOSConfig.h"): [
        ("static allocation", "#define configSUPPORT_STATIC_ALLOCATION 1"),
        ("dynamic allocation off", "#define configSUPPORT_DYNAMIC_ALLOCATION 0"),
        ("tick rate", "#define configTICK_RATE_HZ ((TickType_t)1000U)"),
        ("systick handler alias", "#define xPortSysTickHandler SysTick_Handler"),
        ("pendsv handler alias", "#define xPortPendSVHandler PendSV_Handler"),
        ("svc handler alias", "#define vPortSVCHandler SVC_Handler"),
        ("task notifications enabled", "#define configUSE_TASK_NOTIFICATIONS 1"),
        ("mutex enabled", "#define configUSE_MUTEXES 1"),
        ("recursive mutex disabled", "#define configUSE_RECURSIVE_MUTEXES 0"),
        ("counting semaphore disabled", "#define configUSE_COUNTING_SEMAPHORES 0"),
        ("queue sets disabled", "#define configUSE_QUEUE_SETS 0"),
        ("task delete disabled", "#define INCLUDE_vTaskDelete 0"),
        (
            "max syscall irq priority",
            "#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5",
        ),
    ],
    Path("app/src/app_rtos.c"): [
        ("from-isr task notification", "xTaskNotifyFromISR("),
        ("adxl counting notification", "vTaskNotifyGiveFromISR("),
        ("adxl notification take", "ulTaskNotifyTake(pdTRUE, wait_ticks)"),
        ("task notification wait", "xTaskNotifyWait("),
        ("from-isr yield", "portYIELD_FROM_ISR("),
        ("absolute release check", "app_task_runtime_release_due("),
        ("static event queue", "xQueueCreateStatic("),
        ("static snapshot mutex", "xSemaphoreCreateMutexStatic("),
        ("zero-wait event publish", "xQueueSend(app_rtos_event_queue, event, 0U)"),
        ("zero-wait bounded drain", "xQueueReceive(app_rtos_event_queue, &event, 0U)"),
        ("snapshot zero-wait", "xSemaphoreTake(app_rtos_snapshot_mutex, 0U)"),
        ("health policy evaluation", "app_health_policy_evaluate("),
        ("transition-only health event", "snapshot.decision.publish_transition"),
        ("queue maximum pending", "app_rtos_event_queue_maximum_pending"),
        ("reset reason capture", "app_rtos_capture_reset_reason("),
        ("reset flags cleared after capture", "__HAL_RCC_CLEAR_RESET_FLAGS();"),
        ("retained reset record", ".noinit.app_reset_record"),
        ("reset loop health input", "app_reset_record_loop_latched(&app_rtos_reset_record)"),
        ("health watchdog service", "app_rtos_watchdog_service(&snapshot.decision);"),
        ("shared acquisition timestamp", "const uint32_t now_ms = (uint32_t)xTaskGetTickCount();"),
        ("bme acquisition service", "app_bme280_service(now_ms);"),
        ("bme initialized before scheduler", "app_bme280_initialize();"),
        ("veml acquisition service", "app_veml7700_service(now_ms);"),
        ("veml initialized before scheduler", "app_veml7700_initialize();"),
        ("adxl periodic service", "app_adxl345_service(now_ms, 0U);"),
        ("adxl initialized before scheduler", "app_adxl345_initialize();"),
        ("adxl notifier registered", "bsp_adxl345_register_irq_notifier("),
        ("measurement model initialized", "app_measurement_model_initialize("),
        ("measurement owner publication", "app_rtos_update_measurement_snapshot(now_ms);"),
        ("measurement complete copy", "app_rtos_measurement_snapshot"),
        ("measurement task getter", "app_rtos_get_measurement_snapshot("),
        ("sensor monitor initialized", "app_sensor_monitor_initialize("),
        ("sensor monitor owner update", "app_sensor_monitor_update("),
        ("sensor monitor complete copy", "app_rtos_sensor_monitor_snapshot"),
        ("sensor monitor task getter", "app_rtos_get_sensor_monitor_snapshot("),
        ("modbus aggregate getter", "app_rtos_get_modbus_register_source("),
        ("modbus image generation", "app_rtos_modbus_image_generation"),
        ("health sensor unavailable input", "input.sensor_unavailable_mask ="),
        ("health sensor stale input", "input.sensor_stale_mask ="),
        ("health sensor recovery input", "input.sensor_recovery_mask ="),
        ("modbus transport poll", "app_modbus_transport_poll();"),
        ("partial frame one tick bound", "wait_ticks > (TickType_t)1U"),
        ("legacy smoke compile switch", "#if P5_RS485_LOOPBACK_SMOKE_ENABLE"),
        ("iwdg reset smoke compile switch", "#if P5_IWDG_RESET_SMOKE_ENABLE"),
        ("adxl hil compile switch", "#if P5_ADXL345_HIL_DIAGNOSTIC_ENABLE"),
        ("adxl hil frame schema", '"P5ADXL1 t=%lu st=%s sseq=%lu irq=%lu drop=%lu "'),
        ("adxl hil one second interval", "APP_ADXL345_HIL_REPORT_INTERVAL_MS UINT32_C(1000)"),
        ("adxl hil bounded report count", "APP_ADXL345_HIL_REPORT_LIMIT UINT32_C(180)"),
    ],
    Path("app/src/app_task_model.c"): [
        (
            "adxl hil bounded diagnostic budget",
            "APP_TASK_DIAGNOSTIC_EXECUTION_BUDGET_MS (30U)",
        ),
        (
            "default diagnostic budget retained",
            "APP_TASK_DIAGNOSTIC_EXECUTION_BUDGET_MS (2U)",
        ),
    ],
    Path("app/src/app_boot.c"): [
        ("cycle counter startup", "bsp_clock_cycle_counter_initialize()"),
        ("default modbus transport", "app_modbus_transport_initialize()"),
        ("legacy smoke compile switch", "#if P5_RS485_LOOPBACK_SMOKE_ENABLE"),
        ("iwdg withhold marker", "P5 S3 T05 IWDG WITHHOLD"),
        ("iwdg reset marker", "P5 S3 T05 IWDG RESET OK"),
    ],
    Path("app/include/app_modbus_transport.h"): [
        ("server diagnostics", "p5_modbus_server_diagnostics_t server"),
        ("partial frame query", "app_modbus_transport_has_partial_frame"),
        ("server irq feedback", "app_modbus_transport_on_irq_events"),
    ],
    Path("app/src/app_modbus_transport.c"): [
        ("server process", "p5_modbus_server_process("),
        ("register image provider", "app_modbus_register_image_build("),
        ("response send", "bsp_rs485_send(frame, frame_length)"),
        ("tx complete commit", "p5_modbus_server_on_tx_complete("),
        ("link failure cancel", "p5_modbus_server_on_link_failure("),
    ],
    Path("protocol/include/p5_modbus_rtu_stream.h"): [
        ("stream idle state", "P5_MODBUS_STREAM_IDLE"),
        ("stream receiving state", "P5_MODBUS_STREAM_RECEIVING"),
        ("stream discard state", "P5_MODBUS_STREAM_DISCARD_UNTIL_GAP"),
        ("fixed 256 byte frame", "uint8_t frame[P5_MODBUS_RTU_MAX_ADU_SIZE]"),
    ],
    Path("bsp/include/bsp_rs485_state.h"): [
        ("rx dma chunk 64", "BSP_RS485_RX_DMA_CHUNK_SIZE (64U)"),
        ("tx frame 256", "BSP_RS485_MAX_FRAME_SIZE (256U)"),
        ("rx stop operation", "stop_rx_dma"),
    ],
    Path("bsp/include/bsp_rs485_irq_event.h"): [
        ("idle event kind", "BSP_RS485_RX_EVENT_IDLE"),
        ("dma complete event kind", "BSP_RS485_RX_EVENT_DMA_COMPLETE"),
        ("rx captured cycles", "rx_captured_cycles"),
    ],
    Path("app/include/app_measurement.h"): [
        ("measurement schema revision", "#define APP_MEASUREMENT_SCHEMA_REVISION UINT32_C(1)"),
        ("bme freshness", "#define APP_MEASUREMENT_BME280_FRESH_MS UINT32_C(2500)"),
        ("veml freshness", "#define APP_MEASUREMENT_VEML7700_FRESH_MS UINT32_C(3000)"),
        ("adxl sample freshness", "#define APP_MEASUREMENT_ADXL345_SAMPLE_FRESH_MS UINT32_C(200)"),
        ("adxl feature freshness", "#define APP_MEASUREMENT_ADXL345_FEATURE_FRESH_MS UINT32_C(2500)"),
        ("fresh state", "APP_MEASUREMENT_STATE_FRESH"),
        ("stale state", "APP_MEASUREMENT_STATE_STALE"),
        ("offline state", "APP_MEASUREMENT_STATE_OFFLINE"),
        ("invalid state", "APP_MEASUREMENT_STATE_INVALID"),
        ("sample monotonic time", "uint32_t sample_monotonic_ms;"),
        ("sample age", "uint32_t age_ms;"),
        ("value presence", "bool value_present;"),
        ("retained marker", "bool value_is_retained;"),
        ("logical field start", "APP_MEASUREMENT_FIELD_BME280_TEMPERATURE = 0x0101"),
        ("logical field end", "APP_MEASUREMENT_FIELD_ADXL345_RESULTANT_RMS = 0x0341"),
    ],
    Path("app/src/app_measurement.c"): [
        ("fixed field count", "#define APP_MEASUREMENT_FIELD_COUNT (17U)"),
        ("wrap-safe sample age", "now_ms - metadata->sample_monotonic_ms"),
        ("sequence change admission", "model->observed_sequence[index] != sequence"),
        ("fresh inclusive boundary", "metadata->age_ms <= app_measurement_fresh_limit(source)"),
        ("invalid field unavailable", "if (!source_metadata->value_present)"),
        ("bme full valid admission", "bme_source->valid_mask == BME280_SAMPLE_VALID_ALL"),
        ("feature independent admission", "APP_MEASUREMENT_SOURCE_ADXL345_FEATURE"),
    ],
    Path("app/include/app_sensor_monitor.h"): [
        ("sensor monitor schema revision", "#define APP_SENSOR_MONITOR_SCHEMA_REVISION UINT32_C(1)"),
        ("three monitored devices", "APP_SENSOR_DEVICE_COUNT"),
        ("four source sample stats", "app_sensor_sample_stats_t sample[APP_MEASUREMENT_SOURCE_COUNT];"),
        ("device fault stats", "app_sensor_device_stats_t device[APP_SENSOR_DEVICE_COUNT];"),
        ("unavailable device mask", "uint32_t unavailable_device_mask;"),
        ("stale source mask", "uint32_t stale_source_mask;"),
        ("recovery device mask", "uint32_t recovery_device_mask;"),
        ("monitor update api", "bool app_sensor_monitor_update("),
        ("monitor snapshot api", "bool app_sensor_monitor_get_snapshot("),
    ],
    Path("app/src/app_sensor_monitor.c"): [
        ("wrap-safe sample interval", "metadata->sample_monotonic_ms - stats->last_sample_ms"),
        ("sequence change admission", "stats->last_sequence == metadata->sequence"),
        ("fault episode transition", "fault_active && !stats->fault_active"),
        ("wrap-safe recovery duration", "now_ms - monitor->recovery_started_ms[index]"),
        ("no false physical recovery", "monitor->recovery_started_valid[index] && !offline"),
        ("adxl dropped summary", "adxl345_dropped_sample_lower_bound"),
    ],
    Path("app/include/app_transport_policy.h"): [
        ("event queue depth", "#define APP_TRANSPORT_EVENT_QUEUE_DEPTH (8U)"),
        ("event drain budget", "#define APP_TRANSPORT_EVENT_DRAIN_BUDGET (2U)"),
        ("event timestamp by value", "uint32_t timestamp_ms;"),
        ("event detail by value", "uint32_t detail;"),
        ("event source by value", "uint16_t source;"),
        ("event code by value", "uint16_t code;"),
    ],
    Path("app/include/app_health_policy.h"): [
        ("two-epoch stall gate", "#define APP_HEALTH_STALL_EPOCH_LIMIT (2U)"),
        ("three-attempt recovery gate", "#define APP_HEALTH_RECOVERY_ATTEMPT_LIMIT (3U)"),
        ("feed withheld decision", "APP_WATCHDOG_FEED_WITHHELD"),
        ("feed allowed decision", "APP_WATCHDOG_FEED_ALLOWED"),
        ("sensor unavailable warning", "APP_HEALTH_WARNING_SENSOR_UNAVAILABLE"),
        ("sensor stale warning", "APP_HEALTH_WARNING_SENSOR_STALE"),
        ("sensor recovery warning", "APP_HEALTH_WARNING_SENSOR_RECOVERY"),
        ("sensor unavailable health input", "uint32_t sensor_unavailable_mask;"),
        ("sensor stale health input", "uint32_t sensor_stale_mask;"),
        ("sensor recovery health input", "uint32_t sensor_recovery_mask;"),
    ],
    Path("app/src/app_health_policy.c"): [
        (
            "serviceable degraded recovery feed contract",
            "case APP_HEALTH_SERVICEABLE:\n"
            "    case APP_HEALTH_DEGRADED:\n"
            "    case APP_HEALTH_RECOVERY_REQUIRED:\n"
            "      return APP_WATCHDOG_FEED_ALLOWED;",
        ),
        ("health excludes self progress", "index != (size_t)APP_TASK_HEALTH"),
        ("bounded recovery saturation", "APP_HEALTH_RECOVERY_ATTEMPT_LIMIT"),
        ("sensor unavailable degrades", "warning_mask |= APP_HEALTH_WARNING_SENSOR_UNAVAILABLE;"),
        ("sensor stale degrades", "warning_mask |= APP_HEALTH_WARNING_SENSOR_STALE;"),
        ("sensor recovery degrades", "warning_mask |= APP_HEALTH_WARNING_SENSOR_RECOVERY;"),
    ],
    Path("app/include/app_reset_reason.h"): [
        ("reset record magic", "#define APP_RESET_RECORD_MAGIC UINT32_C(0x50355252)"),
        ("reset loop limit", "#define APP_RESET_LOOP_LIMIT (3U)"),
        ("reset record checksum field", "uint32_t checksum;"),
        ("reset fault persistence", "app_reset_record_note_fault("),
    ],
    Path("bsp/include/bsp_watchdog.h"): [
        ("watchdog refresh api", "bool bsp_watchdog_refresh(void);"),
        ("watchdog debug freeze api", "void bsp_watchdog_enable_debug_freeze(void);"),
    ],
    Path("bsp/src/bsp_watchdog.c"): [
        ("watchdog hal owner", "HAL_IWDG_Refresh(&hiwdg)"),
        ("watchdog debug freeze", "__HAL_DBGMCU_FREEZE_IWDG();"),
    ],
    Path("STM32F446xx_FLASH.ld"): [
        ("retained noinit output", ".noinit (NOLOAD)"),
        ("retained reset input", "KEEP(*(.noinit.app_reset_record))"),
    ],
    Path("sensors/include/bme280.h"): [
        ("bme280 chip identity", "#define BME280_CHIP_ID UINT8_C(0x60)"),
        ("bme280 bmp identity boundary", "#define BME280_BMP280_CHIP_ID UINT8_C(0x58)"),
        ("bme280 one-hz period", "#define BME280_DEFAULT_SAMPLE_PERIOD_MS UINT32_C(1000)"),
        ("bme280 full valid mask", "BME280_SAMPLE_VALID_ALL"),
        ("bme280 integer temperature", "int32_t temperature_centi_c;"),
        ("bme280 integer pressure", "uint32_t pressure_pa;"),
        ("bme280 integer humidity", "uint32_t humidity_milli_pct;"),
    ],
    Path("sensors/src/bme280.c"): [
        ("single recovery attempt", "#define BME280_MAX_RECOVERY_ATTEMPTS (1U)"),
        ("calibration zero guard", "calibration->dig_p1 != 0U"),
        ("humidity clamp", "humidity_value = INT64_C(419430400);"),
        ("ctrl hum before measurement state", "BME280_STATE_CONFIGURE_HUMIDITY"),
    ],
    Path("bsp/include/bsp_spi_bus.h"): [
        ("bounded spi block", "#define BSP_SPI_BUS_MAX_TRANSFER_BYTES (32U)"),
        ("spi block read", "bsp_spi_bus_read_registers("),
        ("spi single write", "bsp_spi_bus_write_register("),
    ],
    Path("app/include/app_bme280.h"): [
        ("bme spi timeout", "#define APP_BME280_SPI_TIMEOUT_MS UINT32_C(5)"),
        ("owner-context snapshot", "Owner-context diagnostic snapshot; cross-task users read app_measurement."),
    ],
    Path("app/src/app_bme280.c"): [
        ("bme block read adapter", "bsp_spi_bus_read_registers("),
        ("bme register write adapter", "bsp_spi_bus_write_register("),
        ("bme default sample period", "BME280_DEFAULT_SAMPLE_PERIOD_MS"),
    ],
    Path("sensors/include/veml7700.h"): [
        ("veml fixed address", "#define VEML7700_ADDRESS_7BIT UINT8_C(0x10)"),
        ("veml default range", "#define VEML7700_DEFAULT_RANGE_LEVEL (2U)"),
        ("veml nine range levels", "#define VEML7700_RANGE_LEVEL_COUNT (9U)"),
        ("veml low threshold", "#define VEML7700_LOW_COUNT_THRESHOLD UINT16_C(100)"),
        ("veml high threshold", "#define VEML7700_HIGH_COUNT_THRESHOLD UINT16_C(10000)"),
        ("veml integer illuminance", "uint32_t illuminance_millilux;"),
        ("veml high lux quality", "VEML7700_QUALITY_HIGH_LUX_UNCORRECTED"),
    ],
    Path("sensors/src/veml7700.c"): [
        ("veml single recovery attempt", "#define VEML7700_MAX_RECOVERY_ATTEMPTS (1U)"),
        ("veml bounded ranging", "#define VEML7700_MAX_RANGING_ADJUSTMENTS (8U)"),
        ("veml finest integer resolution", "UINT16_C(42)"),
        ("veml coarsest integer resolution", "UINT16_C(21504)"),
    ],
    Path("bsp/include/bsp_i2c_bus.h"): [
        ("bounded i2c block", "#define BSP_I2C_BUS_MAX_TRANSFER_BYTES (32U)"),
        ("i2c register write", "bsp_i2c_bus_write_register("),
    ],
    Path("app/include/app_veml7700.h"): [
        ("veml i2c timeout", "#define APP_VEML7700_I2C_TIMEOUT_MS UINT32_C(5)"),
        ("veml owner-context snapshot", "Owner-context diagnostic snapshot; cross-task users read app_measurement."),
    ],
    Path("app/src/app_veml7700.c"): [
        ("veml block read adapter", "bsp_i2c_bus_read_register("),
        ("veml register write adapter", "bsp_i2c_bus_write_register("),
        ("veml default sample period", "VEML7700_DEFAULT_SAMPLE_PERIOD_MS"),
        ("physical recovery not fabricated", "return false;"),
    ],
    Path("sensors/include/adxl345.h"): [
        ("adxl identity", "#define ADXL345_DEVID_VALUE UINT8_C(0xe5)"),
        ("adxl 100 hz", "#define ADXL345_BW_RATE_100_HZ UINT8_C(0x0a)"),
        ("adxl full resolution 4g", "#define ADXL345_DATA_FORMAT_FULL_RES_4G UINT8_C(0x09)"),
        ("adxl 100 sample window", "#define ADXL345_FEATURE_WINDOW_SAMPLES (100U)"),
        ("adxl bounded stall", "#define ADXL345_DATA_READY_STALL_MS UINT32_C(100)"),
        ("adxl accumulator window", "adxl345_feature_window_t window;"),
    ],
    Path("sensors/src/adxl345.c"): [
        ("adxl single recovery", "#define ADXL345_MAX_RECOVERY_ATTEMPTS (1U)"),
        ("adxl integer square root", "adxl345_integer_root("),
        ("adxl bounded state service", "switch (driver->state)"),
        ("adxl coherent read", "ADXL345_DATAX0_REGISTER"),
        ("adxl event coalesce", "event_count - 1U"),
    ],
    Path("app/include/app_adxl345.h"): [
        ("adxl spi timeout", "#define APP_ADXL345_SPI_TIMEOUT_MS UINT32_C(5)"),
        ("adxl owner context snapshot", "Owner-context diagnostic snapshot; cross-task users read app_measurement."),
    ],
    Path("app/src/app_adxl345.c"): [
        ("adxl block read adapter", "bsp_spi_bus_read_registers("),
        ("adxl register write adapter", "bsp_spi_bus_write_register("),
        ("adxl fixed spi device", "BSP_SPI_DEVICE_ADXL345"),
    ],
    Path("bsp/src/bsp_spi_bus.c"): [
        ("adxl multibyte bit", "BSP_SPI_BUS_ADXL345_MULTIBYTE_BIT"),
        ("adxl coherent command", "UINT8_C(0xf2)"),
        ("adxl device specific multibyte", "device == BSP_SPI_DEVICE_ADXL345"),
    ],
    Path("bsp/src/bsp_adxl345_irq.c"): [
        ("adxl hal callback", "void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)"),
        ("adxl pin admission", "gpio_pin != ADXL345_INT1_Pin"),
        ("adxl notifier call", "bsp_adxl345_irq_notifier();"),
    ],
    Path("bsp/src/bsp_rs485.c"): [
        ("irq event publish", "bsp_rs485_irq_publish_from_isr("),
        ("rx metadata publish", "bsp_rs485_irq_publish_rx_from_isr("),
        ("normal dma chunk arm", "BSP_RS485_RX_DMA_CHUNK_SIZE"),
        ("rx stop before tx", "HAL_UART_AbortReceive(&huart1)"),
        ("cycle capture", "bsp_clock_cycle_now()"),
        ("task-context event service", "bsp_rs485_service_irq_events(void)"),
    ],
}


FORBIDDEN_CHECKS: CheckTable = {
    Path("freertos_modbus_can_node.ioc"): [
        ("legacy systick timebase", "VP_SYS_VS_Systick.Mode=SysTick"),
        ("non-rtos priority group", "NVIC.PriorityGroup=NVIC_PRIORITYGROUP_0"),
    ],
    Path("Core/Src/stm32f4xx_it.c"): [
        ("hal tick in systick irq file", "HAL_IncTick();"),
        ("duplicate systick handler", "void SysTick_Handler(void)"),
        ("duplicate pendsv handler", "void PendSV_Handler(void)"),
        ("duplicate svc handler", "void SVC_Handler(void)"),
    ],
    Path("CMakeLists.txt"): [
        ("scheduler smoke default on", "P5_RTOS_SCHEDULER_SMOKE \"Enable the bounded scheduler-start smoke\" ON"),
        ("irq notification smoke default on", "P5_IRQ_NOTIFICATION_SMOKE \"Enable bounded DWT IRQ latency summaries\" ON"),
        ("iwdg reset smoke default on", "P5_IWDG_RESET_SMOKE \"Enable one controlled IWDG reset smoke\" ON"),
        ("adxl hil diagnostic default on", "P5_ADXL345_HIL_DIAGNOSTIC \"Enable bounded ADXL345 HIL diagnostic output\" ON"),
        ("production fault injection option", "P5_FAULT_INJECTION"),
    ],
    Path("config/FreeRTOSConfig.h"): [
        ("task notifications disabled", "#define configUSE_TASK_NOTIFICATIONS 0"),
        ("mutex disabled", "#define configUSE_MUTEXES 0"),
        ("recursive mutex enabled", "#define configUSE_RECURSIVE_MUTEXES 1"),
        ("counting semaphore enabled", "#define configUSE_COUNTING_SEMAPHORES 1"),
        ("queue sets enabled", "#define configUSE_QUEUE_SETS 1"),
        ("task delete enabled", "#define INCLUDE_vTaskDelete 1"),
    ],
    Path("app/src/app_rtos.c"): [
        ("blocking transport wait", "portMAX_DELAY"),
        ("dynamic queue create", "xQueueCreate("),
        ("dynamic mutex create", "xSemaphoreCreateMutex()"),
        ("ordinary event queue from ISR", "xQueueSendFromISR("),
        ("event overwrite", "xQueueOverwrite("),
        ("direct iwdg refresh", "HAL_IWDG_Refresh("),
        ("task delete", "vTaskDelete("),
        ("direct system reset", "NVIC_SystemReset("),
        ("second spi runtime owner", "bsp_spi_bus_"),
        ("second i2c runtime owner", "bsp_i2c_bus_"),
    ],
    Path("Core/Src/main.c"): [
        ("main iwdg refresh", "HAL_IWDG_Refresh("),
    ],
    Path("app/src/app_health_policy.c"): [
        ("unbounded recovery loop", "for (;;)")
    ],
    Path("sensors/src/bme280.c"): [
        ("bme hal dependency", "HAL_"),
        ("bme freertos delay", "vTaskDelay("),
        ("bme snapshot mutex", "xSemaphore"),
        ("bme dynamic allocation", "malloc("),
        ("bme unbounded for loop", "for (;;)"),
        ("bme unbounded true loop", "while (true)"),
        ("bme unbounded one loop", "while (1)"),
        ("bme periodic print", "printf("),
    ],
    Path("app/src/app_bme280.c"): [
        ("bme adapter direct hal", "HAL_"),
        ("bme adapter task delay", "vTaskDelay("),
        ("bme adapter mutex", "xSemaphore"),
        ("bme adapter dynamic allocation", "malloc("),
        ("bme adapter unbounded loop", "for (;;)"),
    ],
    Path("sensors/src/veml7700.c"): [
        ("veml hal dependency", "HAL_"),
        ("veml freertos delay", "vTaskDelay("),
        ("veml snapshot mutex", "xSemaphore"),
        ("veml dynamic allocation", "malloc("),
        ("veml unbounded for loop", "for (;;)"),
        ("veml unbounded true loop", "while (true)"),
        ("veml unbounded one loop", "while (1)"),
        ("veml periodic print", "printf("),
    ],
    Path("app/src/app_veml7700.c"): [
        ("veml adapter direct hal", "HAL_"),
        ("veml adapter task delay", "vTaskDelay("),
        ("veml adapter mutex", "xSemaphore"),
        ("veml adapter dynamic allocation", "malloc("),
        ("veml adapter unbounded loop", "for (;;)"),
    ],
    Path("sensors/src/adxl345.c"): [
        ("adxl hal dependency", "HAL_"),
        ("adxl freertos dependency", "vTask"),
        ("adxl snapshot mutex", "xSemaphore"),
        ("adxl dynamic allocation", "malloc("),
        ("adxl unbounded for loop", "for (;;)"),
        ("adxl unbounded true loop", "while (true)"),
        ("adxl unbounded one loop", "while (1)"),
        ("adxl periodic print", "printf("),
        ("adxl floating square root", "sqrt("),
        ("adxl floating square root f", "sqrtf("),
    ],
    Path("app/src/app_adxl345.c"): [
        ("adxl adapter direct hal", "HAL_"),
        ("adxl adapter task delay", "vTaskDelay("),
        ("adxl adapter mutex", "xSemaphore"),
        ("adxl adapter dynamic allocation", "malloc("),
        ("adxl adapter unbounded loop", "for (;;)"),
    ],
    Path("app/src/app_measurement.c"): [
        ("measurement hal dependency", "HAL_"),
        ("measurement freertos task dependency", "vTask"),
        ("measurement freertos task read", "xTask"),
        ("measurement semaphore dependency", "xSemaphore"),
        ("measurement queue dependency", "xQueue"),
        ("measurement dynamic allocation", "malloc("),
        ("measurement unbounded for loop", "for (;;)"),
        ("measurement unbounded true loop", "while (true)"),
        ("measurement unbounded one loop", "while (1)"),
        ("measurement periodic print", "printf("),
        ("measurement modbus mapping", "MODBUS_"),
        ("measurement can mapping", "CAN_ID"),
    ],
    Path("app/src/app_sensor_monitor.c"): [
        ("sensor monitor hal dependency", "HAL_"),
        ("sensor monitor freertos task dependency", "vTask"),
        ("sensor monitor freertos task read", "xTask"),
        ("sensor monitor semaphore dependency", "xSemaphore"),
        ("sensor monitor queue dependency", "xQueue"),
        ("sensor monitor dynamic allocation", "malloc("),
        ("sensor monitor unbounded for loop", "for (;;)"),
        ("sensor monitor unbounded true loop", "while (true)"),
        ("sensor monitor unbounded one loop", "while (1)"),
        ("sensor monitor periodic print", "printf("),
        ("sensor monitor modbus mapping", "MODBUS_"),
        ("sensor monitor can mapping", "CAN_ID"),
    ],
}


CALLBACKS = (
    "HAL_UART_TxCpltCallback",
    "HAL_UARTEx_RxEventCallback",
    "HAL_UART_ErrorCallback",
)

CALLBACK_FORBIDDEN = (
    "memcpy(",
    "HAL_UARTEx_ReceiveToIdle_DMA(",
    "bsp_rs485_state_on_",
    "bsp_rs485_state_poll(",
    "HAL_UART_Abort",
    "xTaskNotify(",
    "xQueue",
)

ADXL_CALLBACK_FORBIDDEN = (
    "HAL_SPI_",
    "bsp_spi_bus_",
    "memcpy(",
    "vTaskDelay(",
    "xQueue",
    "xSemaphore",
    "printf(",
)


def function_body(content: str, function_name: str) -> str | None:
    match = re.search(
        rf"\bvoid\s+{re.escape(function_name)}\s*\([^)]*\)\s*\{{",
        content,
        re.DOTALL,
    )
    if match is None:
        return None

    start = match.end() - 1
    depth = 0
    for index in range(start, len(content)):
        if content[index] == "{":
            depth += 1
        elif content[index] == "}":
            depth -= 1
            if depth == 0:
                return content[start : index + 1]
    return None


def verify(
    root: Path, overrides: Mapping[Path, str] | None = None
) -> tuple[list[str], int]:
    errors: list[str] = []
    checked = 0
    override_map = overrides or {}

    for relative_path, facts in CHECKS.items():
        if relative_path in override_map:
            content = override_map[relative_path]
        else:
            path = root / relative_path
            try:
                content = path.read_text(encoding="utf-8")
            except (OSError, UnicodeError) as exc:
                errors.append(f"{relative_path}: cannot read: {exc}")
                continue

        for label, snippet in facts:
            checked += 1
            if snippet not in content:
                errors.append(f"{relative_path}: {label}: missing {snippet!r}")

    for relative_path, facts in FORBIDDEN_CHECKS.items():
        if relative_path in override_map:
            content = override_map[relative_path]
        else:
            path = root / relative_path
            try:
                content = path.read_text(encoding="utf-8")
            except (OSError, UnicodeError) as exc:
                errors.append(f"{relative_path}: cannot read: {exc}")
                continue

        for label, snippet in facts:
            checked += 1
            if snippet in content:
                errors.append(f"{relative_path}: {label}: forbidden {snippet!r}")

    measurement_header_path = Path("app/include/app_measurement.h")
    if measurement_header_path in override_map:
        measurement_header = override_map[measurement_header_path]
    else:
        try:
            measurement_header = (root / measurement_header_path).read_text(
                encoding="utf-8"
            )
        except (OSError, UnicodeError) as exc:
            errors.append(
                f"{measurement_header_path}: cannot read field IDs: {exc}"
            )
            measurement_header = ""

    field_ids = re.findall(
        r"APP_MEASUREMENT_FIELD_[A-Z0-9_]+\s*=\s*(0x[0-9a-fA-F]+)",
        measurement_header,
    )
    checked += 1
    if (len(field_ids) != 17) or (len(set(field_ids)) != 17):
        errors.append(
            f"{measurement_header_path}: measurement field ids unique: "
            f"expected 17 unique IDs, got {len(set(field_ids))}"
        )

    rtos_path = Path("app/src/app_rtos.c")
    if rtos_path in override_map:
        rtos_source = override_map[rtos_path]
    else:
        try:
            rtos_source = (root / rtos_path).read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            errors.append(f"{rtos_path}: cannot read mutex count: {exc}")
            rtos_source = ""
    checked += 1
    if rtos_source.count("xSemaphoreCreateMutexStatic(") != 1:
        errors.append(
            f"{rtos_path}: single snapshot mutex: expected exactly one "
            "xSemaphoreCreateMutexStatic call"
        )

    checked += 1
    if rtos_source.count("bsp_watchdog_refresh()") != 1:
        errors.append(
            f"{rtos_path}: single watchdog refresh: expected exactly one "
            "bsp_watchdog_refresh call"
        )
    watchdog_body = function_body(rtos_source, "app_rtos_watchdog_service")
    checked += 1
    if (watchdog_body is None) or (
        watchdog_body.count("bsp_watchdog_refresh()") != 1
    ):
        errors.append(
            f"{rtos_path}: health-owned watchdog refresh: expected one call "
            "inside app_rtos_watchdog_service"
        )
    health_service_body = function_body(rtos_source, "app_rtos_health_service")
    checked += 1
    if (health_service_body is None) or (
        health_service_body.count(
            "app_rtos_watchdog_service(&snapshot.decision);"
        ) != 1
    ):
        errors.append(
            f"{rtos_path}: health task watchdog ownership: expected one "
            "watchdog service call from app_rtos_health_service"
        )

    periodic_body = function_body(
        rtos_source, "app_rtos_acquisition_periodic_service"
    )
    periodic_tokens = (
        "app_adxl345_service(now_ms, 0U);",
        "app_bme280_service(now_ms);",
        "app_veml7700_service(now_ms);",
        "app_rtos_update_measurement_snapshot(now_ms);",
    )
    checked += 1
    if periodic_body is None:
        errors.append(f"{rtos_path}: acquisition periodic body missing")
    else:
        positions: list[int] = []
        for token in periodic_tokens:
            checked += 1
            if periodic_body.count(token) != 1:
                errors.append(
                    f"{rtos_path}: acquisition periodic single-call order: "
                    f"expected one {token!r}"
                )
            positions.append(periodic_body.find(token))
        checked += 1
        if any(position < 0 for position in positions) or positions != sorted(
            positions
        ):
            errors.append(
                f"{rtos_path}: acquisition periodic order: expected "
                "ADXL -> BME -> VEML -> snapshot"
            )

    event_body = function_body(
        rtos_source, "app_rtos_acquisition_event_service"
    )
    checked += 1
    if event_body is None:
        errors.append(f"{rtos_path}: acquisition event body missing")
    else:
        checked += 1
        if event_body.count("app_adxl345_service(now_ms, event_count);") != 1:
            errors.append(
                f"{rtos_path}: acquisition event coalescing: expected one "
                "aggregated ADXL service call"
            )

    health_path = Path("app/src/app_health_policy.c")
    if health_path in override_map:
        health_source = override_map[health_path]
    else:
        try:
            health_source = (root / health_path).read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            errors.append(f"{health_path}: cannot read sensor warning blocks: {exc}")
            health_source = ""
    sensor_blocks = (
        (
            "sensor unavailable local-degrade",
            "if (input->sensor_unavailable_mask != 0U)",
            "if (input->sensor_stale_mask != 0U)",
        ),
        (
            "sensor stale local-degrade",
            "if (input->sensor_stale_mask != 0U)",
            "if (input->sensor_recovery_mask != 0U)",
        ),
        (
            "sensor recovery local-degrade",
            "if (input->sensor_recovery_mask != 0U)",
            "switch (input->recovery_result)",
        ),
    )
    for label, marker, next_marker in sensor_blocks:
        checked += 1
        start = health_source.find(marker)
        end = health_source.find(next_marker, start + len(marker))
        if start < 0 or end < 0:
            errors.append(f"{health_path}: {label}: warning block missing")
            continue
        checked += 1
        if "reset_required" in health_source[start:end]:
            errors.append(
                f"{health_path}: {label}: sensor warning must not request reset"
            )

    callback_path = Path("bsp/src/bsp_rs485.c")
    if callback_path in override_map:
        callback_source = override_map[callback_path]
    else:
        try:
            callback_source = (root / callback_path).read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            errors.append(f"{callback_path}: cannot read callbacks: {exc}")
            callback_source = ""

    for callback in CALLBACKS:
        body = function_body(callback_source, callback)
        checked += 1
        if body is None:
            errors.append(f"{callback_path}: callback body missing: {callback}")
            continue
        checked += 1
        if "bsp_rs485_irq_publish_from_isr(" not in body:
            errors.append(f"{callback_path}: {callback}: event publish missing")
        for forbidden in CALLBACK_FORBIDDEN:
            checked += 1
            if forbidden in body:
                errors.append(
                    f"{callback_path}: {callback}: forbidden {forbidden!r}"
                )

    adxl_callback_path = Path("bsp/src/bsp_adxl345_irq.c")
    if adxl_callback_path in override_map:
        adxl_callback_source = override_map[adxl_callback_path]
    else:
        try:
            adxl_callback_source = (root / adxl_callback_path).read_text(
                encoding="utf-8"
            )
        except (OSError, UnicodeError) as exc:
            errors.append(
                f"{adxl_callback_path}: cannot read callback: {exc}"
            )
            adxl_callback_source = ""

    adxl_callback = function_body(
        adxl_callback_source, "HAL_GPIO_EXTI_Callback"
    )
    checked += 1
    if adxl_callback is None:
        errors.append(
            f"{adxl_callback_path}: callback body missing: "
            "HAL_GPIO_EXTI_Callback"
        )
    else:
        checked += 1
        if "bsp_adxl345_irq_notifier();" not in adxl_callback:
            errors.append(
                f"{adxl_callback_path}: HAL_GPIO_EXTI_Callback: "
                "notifier missing"
            )
        for forbidden in ADXL_CALLBACK_FORBIDDEN:
            checked += 1
            if forbidden in adxl_callback:
                errors.append(
                    f"{adxl_callback_path}: HAL_GPIO_EXTI_Callback: "
                    f"forbidden {forbidden!r}"
                )

    return errors, checked


def print_result(errors: list[str], checked: int) -> int:
    if errors:
        print(f"P5 BSP CONTRACT: FAIL ({len(errors)} of {checked} facts failed)")
        for error in errors:
            print(f"[FAIL] {error}")
        return 1

    print(
        "P5 BSP CONTRACT: PASS "
        f"({checked} stable facts, bounded hardware partial)"
    )
    return 0


def run_self_test(root: Path) -> int:
    errors, checked = verify(root)
    if errors:
        return print_result(errors, checked)

    cmake_path = Path("CMakeLists.txt")
    original = (root / cmake_path).read_text(encoding="utf-8")
    expected = (
        'option(P5_DEVICE_PROBE_SMOKE "Enable the one-shot SPI/I2C device probe" OFF)'
    )
    mutant = original.replace(expected, expected[:-4] + "ON)", 1)
    if mutant == original:
        print("P5 BSP CONTRACT SELF-TEST: FAIL (could not create in-memory mutant)")
        return 2

    mutant_errors, _ = verify(root, {cmake_path: mutant})
    caught = any("device probe smoke default off" in item for item in mutant_errors)
    if not caught:
        print("P5 BSP CONTRACT SELF-TEST: FAIL (unsafe option was not detected)")
        return 2

    ioc_path = Path("freertos_modbus_can_node.ioc")
    original_ioc = (root / ioc_path).read_text(encoding="utf-8")
    legacy_timebase = original_ioc + "\nVP_SYS_VS_Systick.Mode=SysTick\n"
    timebase_errors, _ = verify(root, {ioc_path: legacy_timebase})
    caught_timebase = any(
        "legacy systick timebase" in item for item in timebase_errors
    )
    if not caught_timebase:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(legacy SysTick HAL timebase was not detected)"
        )
        return 2

    invalid_priority_group = original_ioc.replace(
        "NVIC.PriorityGroup=NVIC_PRIORITYGROUP_4",
        "NVIC.PriorityGroup=NVIC_PRIORITYGROUP_0",
        1,
    )
    if invalid_priority_group == original_ioc:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create priority-group mutant)"
        )
        return 2
    priority_group_errors, _ = verify(
        root, {ioc_path: invalid_priority_group}
    )
    caught_priority_group = any(
        "priority group" in item for item in priority_group_errors
    )
    if not caught_priority_group:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(non-RTOS NVIC priority group was not detected)"
        )
        return 2

    irq_priority_mutant = original_ioc.replace(
        "NVIC.USART1_IRQn=true\\:6\\:0",
        "NVIC.USART1_IRQn=true\\:0\\:0",
        1,
    )
    if irq_priority_mutant == original_ioc:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create RTOS IRQ priority mutant)"
        )
        return 2
    irq_priority_errors, _ = verify(root, {ioc_path: irq_priority_mutant})
    caught_irq_priority = any(
        "usart1 rtos irq priority" in item for item in irq_priority_errors
    )
    if not caught_irq_priority:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(unsafe RTOS IRQ priority was not detected)"
        )
        return 2

    callback_path = Path("bsp/src/bsp_rs485.c")
    original_callbacks = (root / callback_path).read_text(encoding="utf-8")
    callback_marker = (
        "void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)\n{\n"
    )
    callback_mutant = original_callbacks.replace(
        callback_marker,
        callback_marker + "  memcpy(0, 0, 0);\n",
        1,
    )
    if callback_mutant == original_callbacks:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create callback-work mutant)"
        )
        return 2
    callback_errors, _ = verify(root, {callback_path: callback_mutant})
    caught_callback_work = any(
        "HAL_UART_TxCpltCallback" in item and "memcpy" in item
        for item in callback_errors
    )
    if not caught_callback_work:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(forbidden callback work was not detected)"
        )
        return 2

    transport_path = Path("app/include/app_transport_policy.h")
    original_transport = (root / transport_path).read_text(encoding="utf-8")
    depth_mutant = original_transport.replace(
        "#define APP_TRANSPORT_EVENT_QUEUE_DEPTH (8U)",
        "#define APP_TRANSPORT_EVENT_QUEUE_DEPTH (0U)",
        1,
    )
    if depth_mutant == original_transport:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create queue-depth mutant)"
        )
        return 2
    depth_errors, _ = verify(root, {transport_path: depth_mutant})
    if not any("event queue depth" in item for item in depth_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(zero queue depth was not detected)"
        )
        return 2

    rtos_path = Path("app/src/app_rtos.c")
    original_rtos = (root / rtos_path).read_text(encoding="utf-8")
    wait_mutant = original_rtos.replace(
        "xQueueSend(app_rtos_event_queue, event, 0U)",
        "xQueueSend(app_rtos_event_queue, event, portMAX_DELAY)",
        1,
    )
    if wait_mutant == original_rtos:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create blocking-wait mutant)"
        )
        return 2
    wait_errors, _ = verify(root, {rtos_path: wait_mutant})
    if not any("blocking transport wait" in item for item in wait_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(blocking transport wait was not detected)"
        )
        return 2

    dynamic_mutant = original_rtos.replace(
        "xQueueCreateStatic(", "xQueueCreate(", 1
    )
    if dynamic_mutant == original_rtos:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create dynamic-queue mutant)"
        )
        return 2
    dynamic_errors, _ = verify(root, {rtos_path: dynamic_mutant})
    if not any("dynamic queue create" in item for item in dynamic_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(dynamic queue create was not detected)"
        )
        return 2

    callback_queue_mutant = original_callbacks.replace(
        callback_marker,
        callback_marker + "  xQueueSend(0, 0, 0);\n",
        1,
    )
    if callback_queue_mutant == original_callbacks:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create callback-queue mutant)"
        )
        return 2
    callback_queue_errors, _ = verify(
        root, {callback_path: callback_queue_mutant}
    )
    if not any(
        "HAL_UART_TxCpltCallback" in item and "xQueue" in item
        for item in callback_queue_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(ordinary callback queue API was not detected)"
        )
        return 2

    health_header_path = Path("app/include/app_health_policy.h")
    original_health_header = (root / health_header_path).read_text(
        encoding="utf-8"
    )
    stall_mutant = original_health_header.replace(
        "#define APP_HEALTH_STALL_EPOCH_LIMIT (2U)",
        "#define APP_HEALTH_STALL_EPOCH_LIMIT (1U)",
        1,
    )
    if stall_mutant == original_health_header:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create one-epoch-stall mutant)"
        )
        return 2
    stall_errors, _ = verify(root, {health_header_path: stall_mutant})
    if not any("two-epoch stall gate" in item for item in stall_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(one-epoch reset gate was not detected)"
        )
        return 2

    recovery_mutant = original_health_header.replace(
        "#define APP_HEALTH_RECOVERY_ATTEMPT_LIMIT (3U)",
        "#define APP_HEALTH_RECOVERY_ATTEMPT_LIMIT (0U)",
        1,
    )
    if recovery_mutant == original_health_header:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create recovery-budget mutant)"
        )
        return 2
    recovery_errors, _ = verify(root, {health_header_path: recovery_mutant})
    if not any(
        "three-attempt recovery gate" in item for item in recovery_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(unbounded/zero recovery budget was not detected)"
        )
        return 2

    health_source_path = Path("app/src/app_health_policy.c")
    original_health_source = (root / health_source_path).read_text(
        encoding="utf-8"
    )
    degraded_feed_mutant = original_health_source.replace(
        "case APP_HEALTH_DEGRADED:",
        "case APP_HEALTH_RESET_REQUIRED:",
        1,
    )
    if degraded_feed_mutant == original_health_source:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create degraded-feed mutant)"
        )
        return 2
    degraded_feed_errors, _ = verify(
        root, {health_source_path: degraded_feed_mutant}
    )
    if not any(
        "serviceable degraded recovery feed contract" in item
        for item in degraded_feed_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(degraded feed regression was not detected)"
        )
        return 2

    config_path = Path("config/FreeRTOSConfig.h")
    original_config = (root / config_path).read_text(encoding="utf-8")
    task_delete_mutant = original_config.replace(
        "#define INCLUDE_vTaskDelete 0",
        "#define INCLUDE_vTaskDelete 1",
        1,
    )
    if task_delete_mutant == original_config:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create task-delete mutant)"
        )
        return 2
    task_delete_errors, _ = verify(root, {config_path: task_delete_mutant})
    if not any("task delete enabled" in item for item in task_delete_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(task delete enable was not detected)"
        )
        return 2

    expected_iwdg_smoke = (
        'option(P5_IWDG_RESET_SMOKE "Enable one controlled IWDG reset smoke" OFF)'
    )
    iwdg_mutant = original.replace(
        expected_iwdg_smoke,
        expected_iwdg_smoke[:-4] + "ON)",
        1,
    )
    if iwdg_mutant == original:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create default-on IWDG smoke mutant)"
        )
        return 2
    iwdg_errors, _ = verify(root, {cmake_path: iwdg_mutant})
    if not any("iwdg reset smoke default on" in item for item in iwdg_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(default-on IWDG smoke was not detected)"
        )
        return 2

    expected_adxl_hil = (
        'option(P5_ADXL345_HIL_DIAGNOSTIC '
        '"Enable bounded ADXL345 HIL diagnostic output" OFF)'
    )
    adxl_hil_mutant = original.replace(
        expected_adxl_hil,
        expected_adxl_hil[:-4] + "ON)",
        1,
    )
    if adxl_hil_mutant == original:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create default-on ADXL345 HIL mutant)"
        )
        return 2
    adxl_hil_errors, _ = verify(root, {cmake_path: adxl_hil_mutant})
    if not any(
        "adxl hil diagnostic default on" in item
        for item in adxl_hil_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(default-on ADXL345 HIL diagnostic was not detected)"
        )
        return 2

    second_feed_mutant = original_rtos + "\nbsp_watchdog_refresh();\n"
    second_feed_errors, _ = verify(root, {rtos_path: second_feed_mutant})
    if not any("single watchdog refresh" in item for item in second_feed_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(second/direct feed owner was not detected)"
        )
        return 2

    sensor_path = Path("sensors/src/bme280.c")
    original_sensor = (root / sensor_path).read_text(encoding="utf-8")
    sensor_delay_mutant = original_sensor + "\nHAL_Delay(1U);\n"
    sensor_delay_errors, _ = verify(root, {sensor_path: sensor_delay_mutant})
    if not any("bme hal dependency" in item for item in sensor_delay_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(BME280 HAL delay/dependency was not detected)"
        )
        return 2

    sensor_loop_mutant = original_sensor + "\nfor (;;) {}\n"
    sensor_loop_errors, _ = verify(root, {sensor_path: sensor_loop_mutant})
    if not any("bme unbounded for loop" in item for item in sensor_loop_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(BME280 unbounded retry loop was not detected)"
        )
        return 2

    second_spi_owner_mutant = original_rtos + "\nbsp_spi_bus_read_register(0, 0, 0, 0);\n"
    second_spi_owner_errors, _ = verify(
        root, {rtos_path: second_spi_owner_mutant}
    )
    if not any(
        "second spi runtime owner" in item for item in second_spi_owner_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(second SPI runtime owner was not detected)"
        )
        return 2

    bme_adapter_path = Path("app/src/app_bme280.c")
    original_bme_adapter = (root / bme_adapter_path).read_text(encoding="utf-8")
    bme_mutex_mutant = original_bme_adapter + "\nxSemaphoreTake(0, 0);\n"
    bme_mutex_errors, _ = verify(
        root, {bme_adapter_path: bme_mutex_mutant}
    )
    if not any("bme adapter mutex" in item for item in bme_mutex_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(SPI-under-snapshot-mutex mutant was not detected)"
        )
        return 2

    veml_sensor_path = Path("sensors/src/veml7700.c")
    original_veml_sensor = (root / veml_sensor_path).read_text(encoding="utf-8")
    veml_delay_mutant = original_veml_sensor + "\nHAL_Delay(1U);\n"
    veml_delay_errors, _ = verify(
        root, {veml_sensor_path: veml_delay_mutant}
    )
    if not any("veml hal dependency" in item for item in veml_delay_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(VEML7700 HAL delay/dependency was not detected)"
        )
        return 2

    veml_loop_mutant = original_veml_sensor + "\nfor (;;) {}\n"
    veml_loop_errors, _ = verify(
        root, {veml_sensor_path: veml_loop_mutant}
    )
    if not any("veml unbounded for loop" in item for item in veml_loop_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(VEML7700 unbounded retry loop was not detected)"
        )
        return 2

    second_i2c_owner_mutant = original_rtos + "\nbsp_i2c_bus_read_register(0, 0, 0, 0, 0, 0);\n"
    second_i2c_owner_errors, _ = verify(
        root, {rtos_path: second_i2c_owner_mutant}
    )
    if not any(
        "second i2c runtime owner" in item for item in second_i2c_owner_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(second I2C runtime owner was not detected)"
        )
        return 2

    veml_adapter_path = Path("app/src/app_veml7700.c")
    original_veml_adapter = (root / veml_adapter_path).read_text(encoding="utf-8")
    veml_mutex_mutant = original_veml_adapter + "\nxSemaphoreTake(0, 0);\n"
    veml_mutex_errors, _ = verify(
        root, {veml_adapter_path: veml_mutex_mutant}
    )
    if not any("veml adapter mutex" in item for item in veml_mutex_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(I2C-under-snapshot-mutex mutant was not detected)"
        )
        return 2

    adxl_priority_mutant = original_ioc.replace(
        "NVIC.EXTI4_IRQn=true\\:6\\:0",
        "NVIC.EXTI4_IRQn=true\\:0\\:0",
        1,
    )
    if adxl_priority_mutant == original_ioc:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create ADXL priority mutant)"
        )
        return 2
    adxl_priority_errors, _ = verify(
        root, {ioc_path: adxl_priority_mutant}
    )
    if not any(
        "adxl exti rtos irq priority" in item
        for item in adxl_priority_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(ADXL priority-0 mutant was not detected)"
        )
        return 2

    adxl_sensor_path = Path("sensors/src/adxl345.c")
    original_adxl_sensor = (root / adxl_sensor_path).read_text(
        encoding="utf-8"
    )
    adxl_recovery_mutant = original_adxl_sensor.replace(
        "#define ADXL345_MAX_RECOVERY_ATTEMPTS (1U)",
        "#define ADXL345_MAX_RECOVERY_ATTEMPTS (2U)",
        1,
    )
    if adxl_recovery_mutant == original_adxl_sensor:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create ADXL recovery mutant)"
        )
        return 2
    adxl_recovery_errors, _ = verify(
        root, {adxl_sensor_path: adxl_recovery_mutant}
    )
    if not any("adxl single recovery" in item for item in adxl_recovery_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(ADXL recovery budget mutant was not detected)"
        )
        return 2

    adxl_loop_errors, _ = verify(
        root, {adxl_sensor_path: original_adxl_sensor + "\nfor (;;) {}\n"}
    )
    if not any("adxl unbounded for loop" in item for item in adxl_loop_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(ADXL unbounded feature mutant was not detected)"
        )
        return 2

    spi_path = Path("bsp/src/bsp_spi_bus.c")
    original_spi = (root / spi_path).read_text(encoding="utf-8")
    adxl_mb_mutant = original_spi.replace(
        "device == BSP_SPI_DEVICE_ADXL345",
        "device == BSP_SPI_DEVICE_BME280",
        1,
    )
    if adxl_mb_mutant == original_spi:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create ADXL multibyte mutant)"
        )
        return 2
    adxl_mb_errors, _ = verify(root, {spi_path: adxl_mb_mutant})
    if not any(
        "adxl device specific multibyte" in item for item in adxl_mb_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(ADXL multibyte mutant was not detected)"
        )
        return 2

    adxl_callback_path = Path("bsp/src/bsp_adxl345_irq.c")
    original_adxl_callback = (root / adxl_callback_path).read_text(
        encoding="utf-8"
    )
    adxl_callback_marker = (
        "void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)\n{\n"
    )
    adxl_callback_mutant = original_adxl_callback.replace(
        adxl_callback_marker,
        adxl_callback_marker + "  memcpy(0, 0, 0);\n",
        1,
    )
    if adxl_callback_mutant == original_adxl_callback:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create ADXL callback mutant)"
        )
        return 2
    adxl_callback_errors, _ = verify(
        root, {adxl_callback_path: adxl_callback_mutant}
    )
    if not any(
        "HAL_GPIO_EXTI_Callback" in item and "memcpy(" in item
        for item in adxl_callback_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(ADXL callback-work mutant was not detected)"
        )
        return 2

    measurement_header_path = Path("app/include/app_measurement.h")
    original_measurement_header = (root / measurement_header_path).read_text(
        encoding="utf-8"
    )
    duplicate_field_mutant = original_measurement_header.replace(
        "APP_MEASUREMENT_FIELD_BME280_PRESSURE = 0x0102",
        "APP_MEASUREMENT_FIELD_BME280_PRESSURE = 0x0101",
        1,
    )
    if duplicate_field_mutant == original_measurement_header:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create measurement field-ID mutant)"
        )
        return 2
    duplicate_field_errors, _ = verify(
        root, {measurement_header_path: duplicate_field_mutant}
    )
    if not any(
        "measurement field ids unique" in item
        for item in duplicate_field_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(duplicate measurement field ID was not detected)"
        )
        return 2

    measurement_source_path = Path("app/src/app_measurement.c")
    original_measurement_source = (root / measurement_source_path).read_text(
        encoding="utf-8"
    )
    invalid_value_mutant = original_measurement_source.replace(
        "if (!source_metadata->value_present)",
        "if (source_metadata->value_present)",
        1,
    )
    if invalid_value_mutant == original_measurement_source:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create invalid-value mutant)"
        )
        return 2
    invalid_value_errors, _ = verify(
        root, {measurement_source_path: invalid_value_mutant}
    )
    if not any(
        "invalid field unavailable" in item for item in invalid_value_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(invalid measurement value leak was not detected)"
        )
        return 2

    measurement_loop_errors, _ = verify(
        root,
        {measurement_source_path: original_measurement_source + "\nfor (;;) {}\n"},
    )
    if not any(
        "measurement unbounded for loop" in item
        for item in measurement_loop_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(measurement unbounded loop was not detected)"
        )
        return 2

    fault_injection_errors, _ = verify(
        root, {cmake_path: original + "\nP5_FAULT_INJECTION\n"}
    )
    if not any(
        "production fault injection option" in item
        for item in fault_injection_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(production fault-injection mutant was not detected)"
        )
        return 2

    sensor_monitor_path = Path("app/src/app_sensor_monitor.c")
    original_sensor_monitor = (root / sensor_monitor_path).read_text(
        encoding="utf-8"
    )
    sensor_monitor_loop_errors, _ = verify(
        root,
        {sensor_monitor_path: original_sensor_monitor + "\nfor (;;) {}\n"},
    )
    if not any(
        "sensor monitor unbounded for loop" in item
        for item in sensor_monitor_loop_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(sensor monitor loop mutant was not detected)"
        )
        return 2

    sensor_reset_mutant = original_health_source.replace(
        "warning_mask |= APP_HEALTH_WARNING_SENSOR_UNAVAILABLE;",
        "warning_mask |= APP_HEALTH_WARNING_SENSOR_UNAVAILABLE;\n"
        "    reset_required = true;",
        1,
    )
    if sensor_reset_mutant == original_health_source:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create sensor-reset mutant)"
        )
        return 2
    sensor_reset_errors, _ = verify(
        root, {health_source_path: sensor_reset_mutant}
    )
    if not any(
        "sensor unavailable local-degrade" in item
        for item in sensor_reset_errors
    ):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(sensor reset-escalation mutant was not detected)"
        )
        return 2

    print(
        "P5 BSP CONTRACT SELF-TEST: PASS "
        "(unsafe-option, legacy-SysTick, priority-group, IRQ-priority and "
        "callback-work, queue-depth, blocking-wait, dynamic-queue and "
        "callback-queue, one-epoch-stall, recovery-budget, degraded-feed, "
        "task-delete, default-on-reset-smoke, default-on-adxl-hil, second-feed, "
        "BME-HAL-delay, BME-loop, "
        "second-SPI-owner, BME-mutex, VEML-HAL-delay, VEML-loop, "
        "second-I2C-owner, VEML-mutex, ADXL-priority, ADXL-recovery, "
        "ADXL-loop, ADXL-multibyte, ADXL-callback, measurement-field-ID, "
        "measurement-invalid-value, measurement-loop, production-fault-"
        "injection, sensor-monitor-loop and sensor-reset mutants rejected)"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root (defaults to the script's parent repository)",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="also verify that an unsafe in-memory option mutation is rejected",
    )
    args = parser.parse_args()
    root = args.root.resolve()

    if args.self_test:
        return run_self_test(root)

    errors, checked = verify(root)
    return print_result(errors, checked)


if __name__ == "__main__":
    raise SystemExit(main())
