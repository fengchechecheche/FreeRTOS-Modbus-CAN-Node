cmake_minimum_required(VERSION 3.22)

project(freertos_modbus_can_node LANGUAGES C ASM)

option(P5_BUILD_HOST_TESTS "Build host-only infrastructure tests" ON)
option(P5_RS485_LOOPBACK_SMOKE "Enable the bounded PA9/PA10 loopback smoke" OFF)
option(P5_DEVICE_PROBE_SMOKE "Enable the one-shot SPI/I2C device probe" OFF)
option(P5_RTOS_SCHEDULER_SMOKE "Enable the bounded scheduler-start smoke" OFF)
option(P5_IRQ_NOTIFICATION_SMOKE "Enable bounded DWT IRQ latency summaries" OFF)

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

    add_executable(p5_host_clock
        test/host/clock_main.c
        bsp/src/bsp_clock_math.c)
    target_include_directories(p5_host_clock PRIVATE bsp/include)
    p5_apply_project_warnings(p5_host_clock)
    add_test(NAME p5.host.clock COMMAND p5_host_clock)

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
    target_include_directories(p5_host_resource_budget PRIVATE app/include)
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
  endif()
endif()
