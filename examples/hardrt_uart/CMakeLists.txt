cmake_minimum_required(VERSION 3.20)
project(das_hardrt_uart LANGUAGES C)

# HardRT owns SysTick and PendSV in this application. DAS still supplies the
# reset path, linker policy and default vector table; the strong HardRT handler
# symbols replace DAS's weak core-handler defaults.
set(DAS_USE_DEFAULT_VECTOR_TABLE ON)
find_package(DAS 0.1 CONFIG REQUIRED)
find_package(HardRT 0.5 CONFIG REQUIRED)

if(NOT DAS_PACKAGE_CORE STREQUAL "cm7")
    message(FATAL_ERROR "The current HardRT+DAS reference example targets CM7")
endif()

add_executable(das_hardrt_uart main.c)
# Keep HardRT before DAS in static-library order. The explicit undefined
# handler symbols ensure the scheduler-owned Cortex-M handler objects are
# extracted from libhardrt.a before DAS weak fallbacks can satisfy them.
target_link_libraries(das_hardrt_uart PRIVATE HardRT::hardrt das::das)
target_compile_features(das_hardrt_uart PRIVATE c_std_11)
target_compile_options(das_hardrt_uart PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_hardrt_uart PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-u,PendSV_Handler
    -Wl,-u,SysTick_Handler
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_hardrt_uart.map)
set_target_properties(das_hardrt_uart PROPERTIES SUFFIX ".elf")
