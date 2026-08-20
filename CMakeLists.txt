cmake_minimum_required(VERSION 3.22)

project(freertos_modbus_can_node LANGUAGES C ASM)

option(P5_BUILD_HOST_TESTS "Build host-only infrastructure tests" ON)
option(P5_RS485_LOOPBACK_SMOKE "Enable the bounded PA9/PA10 loopback smoke" OFF)
option(P5_DEVICE_PROBE_SMOKE "Enable the one-shot SPI/I2C device probe" OFF)
option(P5_RTOS_SCHEDULER_SMOKE "Enable the bounded scheduler-start smoke" OFF)
option(P5_IRQ_NOTIFICATION_SMOKE "Enable bounded DWT IRQ latency summaries" OFF)
option(P5_IWDG_RESET_SMOKE "Enable one controlled IWDG reset smoke" OFF)
option(P5_ADXL345_HIL_DIAGNOSTIC "Enable bounded ADXL345 HIL diagnostic output" OFF)
option(P5_ADXL345_INT2_ROUTE_DIAGNOSTIC "Route ADXL345 DATA_READY to INT2 for bounded HIL diagnosis" OFF)
option(P5_ADXL345_POLLING_DIAGNOSTIC "Poll ADXL345 DATA_READY for bounded HIL diagnosis" OFF)
option(P5_CAN_ACK_RX_DIAGNOSTIC "Enable one-shot host-to-STM32 ACK diagnosis" OFF)
option(P5_CAN_ACK_TX_DIAGNOSTIC "Enable one-shot STM32-to-host ACK diagnosis" OFF)

if(P5_CAN_ACK_RX_DIAGNOSTIC AND P5_CAN_ACK_TX_DIAGNOSTIC)
  message(FATAL_ERROR "CAN ACK RX and TX diagnostics are mutually exclusive")
endif()

if(P5_ADXL345_INT2_ROUTE_DIAGNOSTIC AND NOT P5_ADXL345_HIL_DIAGNOSTIC)
  message(FATAL_ERROR "P5_ADXL345_INT2_ROUTE_DIAGNOSTIC requires P5_ADXL345_HIL_DIAGNOSTIC=ON")
endif()

if(P5_ADXL345_POLLING_DIAGNOSTIC AND NOT P5_ADXL345_HIL_DIAGNOSTIC)
  message(FATAL_ERROR "P5_ADXL345_POLLING_DIAGNOSTIC requires P5_ADXL345_HIL_DIAGNOSTIC=ON")
endif()

if(P5_ADXL345_POLLING_DIAGNOSTIC AND P5_ADXL345_INT2_ROUTE_DIAGNOSTIC)
  message(FATAL_ERROR "ADXL345 polling and INT2 route diagnostics are mutually exclusive")
endif()

if(CMAKE_CROSSCOMPILING)
  if(P5_BUILD_HOST_TESTS)
    message(FATAL_ERROR "Host tests must not be built with the firmware toolchain")
  endif()
  include(cmake/firmware.cmake)
else()
  if(P5_BUILD_HOST_TESTS)
    include(CTest)
    include(cmake/project_options.cmake)

    add_executable(p5_host_smoke test/host/smoke_main.c)
    p5_apply_project_warnings(p5_host_smoke)
    add_test(NAME p5.host.smoke COMMAND p5_host_smoke)

    add_executable(p5_host_can_contract
        test/host/can_contract_main.c
        protocol/src/p5_can_contract.c)
    target_include_directories(p5_host_can_contract PRIVATE protocol/include)
    p5_apply_project_warnings(p5_host_can_contract)
    add_test(NAME p5.host.can_contract COMMAND p5_host_can_contract)

    add_executable(p5_host_can_runtime
        test/host/can_runtime_main.c
        bsp/src/bsp_can_irq_event.c
        app/src/app_can_runtime.c
        protocol/src/p5_can_contract.c)
    target_include_directories(p5_host_can_runtime PRIVATE
        app/include bsp/include protocol/include sensors/include)
    p5_apply_project_warnings(p5_host_can_runtime)
    add_test(NAME p5.host.can_runtime COMMAND p5_host_can_runtime)

    add_executable(p5_host_can_ack_diagnostic
        test/host/can_ack_diagnostic_main.c
        app/src/app_can_ack_diagnostic.c)
    target_include_directories(p5_host_can_ack_diagnostic PRIVATE app/include)
    p5_apply_project_warnings(p5_host_can_ack_diagnostic)
    add_test(NAME p5.host.can_ack_diagnostic
             COMMAND p5_host_can_ack_diagnostic)

    add_executable(p5_host_clock
        test/host/clock_main.c
        bsp/src/bsp_clock_math.c)
    target_include_directories(p5_host_clock PRIVATE bsp/include)
    p5_apply_project_warnings(p5_host_clock)
    add_test(NAME p5.host.clock COMMAND p5_host_clock)

    add_executable(p5_host_modbus_codec
        test/host/modbus_codec_main.c
        protocol/src/p5_modbus_crc16.c
        protocol/src/p5_modbus_rtu_adu.c
        protocol/src/p5_modbus_rtu_timing.c)
    target_include_directories(p5_host_modbus_codec PRIVATE protocol/include)
    p5_apply_project_warnings(p5_host_modbus_codec)
    add_test(NAME p5.host.modbus_codec COMMAND p5_host_modbus_codec)

    add_executable(p5_host_modbus_stream
        test/host/modbus_stream_main.c
        protocol/src/p5_modbus_crc16.c
        protocol/src/p5_modbus_rtu_adu.c
        protocol/src/p5_modbus_rtu_stream.c)
    target_include_directories(p5_host_modbus_stream PRIVATE protocol/include)
    p5_apply_project_warnings(p5_host_modbus_stream)
    add_test(NAME p5.host.modbus_stream COMMAND p5_host_modbus_stream)

    add_executable(p5_host_modbus_server
        test/host/modbus_server_main.c
        protocol/src/p5_modbus_crc16.c
        protocol/src/p5_modbus_rtu_adu.c
        protocol/src/p5_modbus_server.c)
    target_include_directories(p5_host_modbus_server PRIVATE protocol/include)
    p5_apply_project_warnings(p5_host_modbus_server)
    add_test(NAME p5.host.modbus_server COMMAND p5_host_modbus_server)

    if(UNIX)
      find_package(Python3 REQUIRED COMPONENTS Interpreter)
      add_executable(p5_host_modbus_pty_slave
          test/host/modbus_pty_slave_main.c
          app/src/app_modbus_register_image.c
          protocol/src/p5_modbus_crc16.c
          protocol/src/p5_modbus_rtu_adu.c
          protocol/src/p5_modbus_rtu_stream.c
          protocol/src/p5_modbus_rtu_timing.c
          protocol/src/p5_modbus_server.c)
      target_include_directories(p5_host_modbus_pty_slave PRIVATE
          app/include protocol/include sensors/include)
      p5_apply_project_warnings(p5_host_modbus_pty_slave)
      add_test(NAME p5.host.modbus_pty
          COMMAND ${Python3_EXECUTABLE}
                  ${CMAKE_SOURCE_DIR}/tools/modbus_pty_test.py
                  --server $<TARGET_FILE:p5_host_modbus_pty_slave>)
      set_tests_properties(p5.host.modbus_pty PROPERTIES TIMEOUT 15)
    endif()

    add_executable(p5_host_modbus_register_image
        test/host/modbus_register_image_main.c
        app/src/app_modbus_register_image.c)
    target_include_directories(p5_host_modbus_register_image PRIVATE
        app/include protocol/include sensors/include)
    p5_apply_project_warnings(p5_host_modbus_register_image)
    add_test(NAME p5.host.modbus_register_image
             COMMAND p5_host_modbus_register_image)

    add_executable(p5_host_rs485
        test/host/rs485_main.c
        bsp/src/bsp_rs485_state.c
        app/src/app_rs485_smoke_logic.c)
    target_include_directories(p5_host_rs485 PRIVATE app/include bsp/include)
    p5_apply_project_warnings(p5_host_rs485)
    add_test(NAME p5.host.rs485 COMMAND p5_host_rs485)

    add_executable(p5_host_device_probe
        test/host/device_probe_main.c
        app/src/app_device_probe_logic.c)
    target_include_directories(p5_host_device_probe PRIVATE app/include)
    p5_apply_project_warnings(p5_host_device_probe)
    add_test(NAME p5.host.device_probe COMMAND p5_host_device_probe)

    add_executable(p5_host_task_model
        test/host/task_model_main.c
        app/src/app_task_model.c)
    target_include_directories(p5_host_task_model PRIVATE app/include)
    p5_apply_project_warnings(p5_host_task_model)
    add_test(NAME p5.host.task_model COMMAND p5_host_task_model)

    add_executable(p5_host_resource_budget
        test/host/resource_budget_main.c)
    target_include_directories(p5_host_resource_budget PRIVATE
        app/include bsp/include protocol/include sensors/include)
    p5_apply_project_warnings(p5_host_resource_budget)
    add_test(NAME p5.host.resource_budget COMMAND p5_host_resource_budget)

    add_executable(p5_host_irq_notification
        test/host/irq_notification_main.c
        bsp/src/bsp_rs485_irq_event.c
        app/src/app_task_model.c)
    target_include_directories(p5_host_irq_notification PRIVATE
        app/include bsp/include)
    p5_apply_project_warnings(p5_host_irq_notification)
    add_test(NAME p5.host.irq_notification COMMAND p5_host_irq_notification)

    add_executable(p5_host_ownership
        test/host/ownership_main.c
        app/src/app_transport_policy.c)
    target_include_directories(p5_host_ownership PRIVATE app/include)
    p5_apply_project_warnings(p5_host_ownership)
    add_test(NAME p5.host.ownership COMMAND p5_host_ownership)

    add_executable(p5_host_health
        test/host/health_main.c
        app/src/app_health_policy.c
        app/src/app_reset_reason.c)
    target_include_directories(p5_host_health PRIVATE app/include)
    p5_apply_project_warnings(p5_host_health)
    add_test(NAME p5.host.health COMMAND p5_host_health)

    add_executable(p5_host_bme280
        test/host/bme280_main.c
        sensors/src/bme280.c)
    target_include_directories(p5_host_bme280 PRIVATE sensors/include)
    p5_apply_project_warnings(p5_host_bme280)
    add_test(NAME p5.host.bme280 COMMAND p5_host_bme280)

    add_executable(p5_host_veml7700
        test/host/veml7700_main.c
        sensors/src/veml7700.c)
    target_include_directories(p5_host_veml7700 PRIVATE sensors/include)
    p5_apply_project_warnings(p5_host_veml7700)
    add_test(NAME p5.host.veml7700 COMMAND p5_host_veml7700)

    add_executable(p5_host_adxl345
        test/host/adxl345_main.c
        sensors/src/adxl345.c)
    target_include_directories(p5_host_adxl345 PRIVATE sensors/include)
    p5_apply_project_warnings(p5_host_adxl345)
    add_test(NAME p5.host.adxl345 COMMAND p5_host_adxl345)

    add_executable(p5_host_measurement
        test/host/app_measurement_main.c
        app/src/app_measurement.c)
    target_include_directories(p5_host_measurement PRIVATE
        app/include sensors/include)
    p5_apply_project_warnings(p5_host_measurement)
    add_test(NAME p5.host.measurement COMMAND p5_host_measurement)

    add_executable(p5_host_sensor_matrix
        test/host/sensor_matrix_main.c
        app/src/app_sensor_monitor.c
        app/src/app_measurement.c
        app/src/app_health_policy.c
        app/src/app_task_model.c
        app/src/app_transport_policy.c
        sensors/src/adxl345.c
        sensors/src/bme280.c
        sensors/src/veml7700.c)
    target_include_directories(p5_host_sensor_matrix PRIVATE
        app/include sensors/include)
    p5_apply_project_warnings(p5_host_sensor_matrix)
    add_test(NAME p5.host.sensor_matrix COMMAND p5_host_sensor_matrix)

    add_executable(p5_host_dual_bus_fault_matrix
        test/host/dual_bus_fault_matrix_main.c
        app/src/app_can_runtime.c
        app/src/app_task_model.c
        app/src/app_transport_policy.c
        app/src/app_health_policy.c
        bsp/src/bsp_rs485_state.c
        protocol/src/p5_modbus_crc16.c
        protocol/src/p5_modbus_rtu_adu.c
        protocol/src/p5_modbus_rtu_stream.c
        protocol/src/p5_can_contract.c)
    target_include_directories(p5_host_dual_bus_fault_matrix PRIVATE
        app/include bsp/include protocol/include sensors/include)
    p5_apply_project_warnings(p5_host_dual_bus_fault_matrix)
    add_test(NAME p5.host.dual_bus_fault_matrix
             COMMAND p5_host_dual_bus_fault_matrix)
  endif()
endif()
