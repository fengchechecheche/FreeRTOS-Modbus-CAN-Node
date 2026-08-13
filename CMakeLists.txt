cmake_minimum_required(VERSION 3.22)

project(freertos_modbus_can_node LANGUAGES C ASM)

option(P5_BUILD_HOST_TESTS "Build host-only infrastructure tests" ON)

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
  endif()
endif()
