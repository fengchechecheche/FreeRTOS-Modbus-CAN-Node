set(P5_FIRMWARE_TARGET freertos_modbus_can_node_firmware)

set(P5_CUBEMX_APPLICATION_SOURCES
    Core/Src/main.c
    Core/Src/gpio.c
    Core/Src/can.c
    Core/Src/i2c.c
    Core/Src/spi.c
    Core/Src/usart.c
    Core/Src/stm32f4xx_it.c
    Core/Src/stm32f4xx_hal_msp.c
    Core/Src/system_stm32f4xx.c
    startup_stm32f446xx.s)

set(P5_STM32F4_HAL_SOURCES
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_can.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c)

set(P5_PROJECT_FIRMWARE_SOURCES
    app/src/app_boot.c
    bsp/src/bsp_clock.c
    bsp/src/bsp_spi_bus.c
    bsp/src/bsp_i2c_bus.c
    bsp/src/bsp_rs485.c
    bsp/src/bsp_can.c)

add_executable(${P5_FIRMWARE_TARGET}
    ${P5_CUBEMX_APPLICATION_SOURCES}
    ${P5_STM32F4_HAL_SOURCES}
    ${P5_PROJECT_FIRMWARE_SOURCES})

set_target_properties(${P5_FIRMWARE_TARGET} PROPERTIES
    OUTPUT_NAME freertos_modbus_can_node
    SUFFIX ".elf")

target_include_directories(${P5_FIRMWARE_TARGET} PRIVATE
    Core/Inc
    app/include
    bsp/include
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include)

target_compile_definitions(${P5_FIRMWARE_TARGET} PRIVATE
    USE_HAL_DRIVER
    STM32F446xx)

target_compile_options(${P5_FIRMWARE_TARGET} PRIVATE
    -ffunction-sections
    -fdata-sections
    -fno-common
    $<$<CONFIG:Debug>:-Og;-g3>
    $<$<CONFIG:Release>:-Os;-g0>)

target_link_options(${P5_FIRMWARE_TARGET} PRIVATE
    --specs=nano.specs
    --specs=nosys.specs
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F446xx_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/freertos_modbus_can_node.map)

target_link_libraries(${P5_FIRMWARE_TARGET} PRIVATE m)

find_program(P5_ARM_OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(P5_ARM_SIZE arm-none-eabi-size REQUIRED)

add_custom_command(TARGET ${P5_FIRMWARE_TARGET} POST_BUILD
    COMMAND ${P5_ARM_OBJCOPY} -O ihex
            $<TARGET_FILE:${P5_FIRMWARE_TARGET}>
            ${CMAKE_CURRENT_BINARY_DIR}/freertos_modbus_can_node.hex
    COMMAND ${P5_ARM_OBJCOPY} -O binary
            $<TARGET_FILE:${P5_FIRMWARE_TARGET}>
            ${CMAKE_CURRENT_BINARY_DIR}/freertos_modbus_can_node.bin
    COMMAND ${P5_ARM_SIZE} $<TARGET_FILE:${P5_FIRMWARE_TARGET}>
    VERBATIM)
