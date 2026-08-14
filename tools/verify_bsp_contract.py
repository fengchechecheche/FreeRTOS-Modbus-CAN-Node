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
        ("usart1 rx dma", "Dma.USART1_RX.0.Instance=DMA2_Stream2"),
        ("usart1 tx dma", "Dma.USART1_TX.1.Instance=DMA2_Stream7"),
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
    ],
    Path("Core/Inc/stm32f4xx_hal_conf.h"): [
        ("tim hal enabled", "#define HAL_TIM_MODULE_ENABLED"),
        ("iwdg hal disabled", "/* #define HAL_IWDG_MODULE_ENABLED */"),
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
        ("app init", "app_boot_initialize()"),
    ],
    Path("Core/Src/gpio.c"): [
        ("adxl cs safe", "HAL_GPIO_WritePin(ADXL345_CS_GPIO_Port, ADXL345_CS_Pin, GPIO_PIN_SET);"),
        ("rs485 de safe", "HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET);"),
        ("bme cs safe", "HAL_GPIO_WritePin(BME280_CS_GPIO_Port, BME280_CS_Pin, GPIO_PIN_SET);"),
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
        ("transport policy source", "app/src/app_transport_policy.c"),
        ("health policy source", "app/src/app_health_policy.c"),
        ("reset reason source", "app/src/app_reset_reason.c"),
    ],
    Path("bsp/include/bsp_clock.h"): [
        ("clock expected sysclk", "BSP_CLOCK_EXPECTED_SYSCLK_HZ UINT32_C(180000000)"),
        ("clock tick api", "bsp_clock_tick_ms(void)"),
        ("clock profile api", "bsp_clock_get_profile(void)"),
        ("clock elapsed api", "bsp_clock_elapsed_ms"),
    ],
    Path("bsp/include/bsp_rs485.h"): [
        ("rs485 init api", "bsp_rs485_initialize(void)"),
        ("rs485 send api", "bsp_rs485_send"),
        ("rs485 receive api", "bsp_rs485_take_received"),
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
        ("ownership host test", "add_test(NAME p5.host.ownership"),
        ("health host test", "add_test(NAME p5.host.health"),
        ("bme280 host test", "add_test(NAME p5.host.bme280"),
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
        ("measurement deferred", "measurement schema deferred to S4"),
        ("command deferred", "schema and instance deferred to S5"),
    ],
    Path("docs/health_recovery_report.md"): [
        (
            "s3 software gate",
            "S3 software gate: `PASS_HOST + PASS_CROSS_BUILD + READY_FOR_HARDWARE`",
        ),
        ("iwdg not configured", "IWDG runtime: `NOT_CONFIGURED / NOT_RUN`"),
        (
            "reset persistence not implemented",
            "Reset-record persistence: `NOT_IMPLEMENTED / NOT_RUN`",
        ),
        ("health hardware waiting", "Hardware status: `WAITING_FOR_HARDWARE`"),
        ("single feed owner", "the only owner of the watchdog feed decision"),
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
        ("bme acquisition service", "app_bme280_service((uint32_t)xTaskGetTickCount())"),
        ("bme initialized before scheduler", "app_bme280_initialize();"),
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
    ],
    Path("app/include/app_reset_reason.h"): [
        ("reset record magic", "#define APP_RESET_RECORD_MAGIC UINT32_C(0x50355252)"),
        ("reset loop limit", "#define APP_RESET_LOOP_LIMIT (3U)"),
        ("reset record checksum field", "uint32_t checksum;"),
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
        ("owner-context snapshot", "Owner-context only until P5-S4-T04"),
    ],
    Path("app/src/app_bme280.c"): [
        ("bme block read adapter", "bsp_spi_bus_read_registers("),
        ("bme register write adapter", "bsp_spi_bus_write_register("),
        ("bme default sample period", "BME280_DEFAULT_SAMPLE_PERIOD_MS"),
    ],
    Path("bsp/src/bsp_rs485.c"): [
        ("irq event publish", "bsp_rs485_irq_publish_from_isr("),
        ("task-context event service", "bsp_rs485_service_irq_events(void)"),
    ],
}


FORBIDDEN_CHECKS: CheckTable = {
    Path("freertos_modbus_can_node.ioc"): [
        ("legacy systick timebase", "VP_SYS_VS_Systick.Mode=SysTick"),
        ("non-rtos priority group", "NVIC.PriorityGroup=NVIC_PRIORITYGROUP_0"),
        ("iwdg cubemx configuration", "IWDG."),
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
        ("watchdog adapter refresh", "bsp_watchdog_refresh("),
        ("task delete", "vTaskDelete("),
        ("direct system reset", "NVIC_SystemReset("),
        ("second spi runtime owner", "bsp_spi_bus_"),
    ],
    Path("Core/Src/main.c"): [
        ("generated iwdg init", "MX_IWDG_Init("),
        ("main iwdg refresh", "HAL_IWDG_Refresh("),
    ],
    Path("Core/Inc/stm32f4xx_hal_conf.h"): [
        ("iwdg hal enabled", "#define HAL_IWDG_MODULE_ENABLED\n"),
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

    return errors, checked


def print_result(errors: list[str], checked: int) -> int:
    if errors:
        print(f"P5 BSP CONTRACT: FAIL ({len(errors)} of {checked} facts failed)")
        for error in errors:
            print(f"[FAIL] {error}")
        return 1

    print(
        "P5 BSP CONTRACT: PASS "
        f"({checked} stable facts, candidate-only, hardware waiting)"
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

    hal_config_path = Path("Core/Inc/stm32f4xx_hal_conf.h")
    original_hal_config = (root / hal_config_path).read_text(encoding="utf-8")
    iwdg_mutant = original_hal_config.replace(
        "/* #define HAL_IWDG_MODULE_ENABLED */",
        "#define HAL_IWDG_MODULE_ENABLED",
        1,
    )
    if iwdg_mutant == original_hal_config:
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(could not create default-IWDG mutant)"
        )
        return 2
    iwdg_errors, _ = verify(root, {hal_config_path: iwdg_mutant})
    if not any("iwdg hal enabled" in item for item in iwdg_errors):
        print(
            "P5 BSP CONTRACT SELF-TEST: FAIL "
            "(default IWDG enable was not detected)"
        )
        return 2

    second_feed_mutant = original_rtos + "\nHAL_IWDG_Refresh(0);\n"
    second_feed_errors, _ = verify(root, {rtos_path: second_feed_mutant})
    if not any("direct iwdg refresh" in item for item in second_feed_errors):
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

    print(
        "P5 BSP CONTRACT SELF-TEST: PASS "
        "(unsafe-option, legacy-SysTick, priority-group, IRQ-priority and "
        "callback-work, queue-depth, blocking-wait, dynamic-queue and "
        "callback-queue, one-epoch-stall, recovery-budget, degraded-feed, "
        "task-delete, default-IWDG, second-feed, BME-HAL-delay, BME-loop, "
        "second-SPI-owner and BME-mutex mutants rejected)"
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
