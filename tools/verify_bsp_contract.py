#!/usr/bin/env python3
"""Check stable P5 BSP candidate facts without claiming hardware validation."""

from __future__ import annotations

import argparse
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
        ("usart1 rx dma", "Dma.USART1_RX.0.Instance=DMA2_Stream2"),
        ("usart1 tx dma", "Dma.USART1_TX.1.Instance=DMA2_Stream7"),
        ("spi1 mode", "SPI1.Mode=SPI_MODE_MASTER"),
        ("spi1 polarity", "SPI1.CLKPolarity=SPI_POLARITY_HIGH"),
        ("spi1 phase", "SPI1.CLKPhase=SPI_PHASE_2EDGE"),
        ("spi1 prescaler", "SPI1.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_32"),
        ("can1 baud", "CAN1.CalculateBaudRate=500000"),
        ("can1 prescaler", "CAN1.Prescaler=6"),
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
    ],
    Path("docs/bsp_contract.md"): [
        ("candidate boundary", "BSP_CONTRACT_CANDIDATE_FROZEN"),
        ("hardware waiting", "WAITING_FOR_HARDWARE"),
        ("hardware upgrade boundary", "BSP_CONTRACT_HARDWARE_FROZEN"),
    ],
}


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

    print(
        "P5 BSP CONTRACT SELF-TEST: PASS "
        "(in-memory device-probe-default-ON mutant rejected)"
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
