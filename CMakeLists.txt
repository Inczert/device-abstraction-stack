cmake_minimum_required(VERSION 3.20)

# DAS_CORE must be known before project() so the toolchain can select the
# correct CPU/FPU flags while the compiler is initialized.
set(DAS_CORE "cm7" CACHE STRING "CPU core selected for a multi-core DAS device")
set_property(CACHE DAS_CORE PROPERTY STRINGS cm7 cm4)

project(das VERSION 0.1.0 LANGUAGES C ASM)

set(DAS_DEVICE "nucleo_h755zi_q" CACHE STRING "Target DAS device")
set_property(CACHE DAS_DEVICE PROPERTY STRINGS nucleo_h755zi_q)
set(STM32_CUBE_H7_DIR "" CACHE PATH "Path to STM32CubeH7 checkout")
set(DAS_LINKER_SCRIPT "" CACHE FILEPATH
    "Override the selected device/core linker script; empty uses the DAS default")
option(DAS_BUILD_HARDWARE_TESTS "Build physical board test firmware" OFF)
option(DAS_BUILD_LINK_TESTS "Build non-executed linker-layout test firmware" OFF)

if(DAS_DEVICE STREQUAL "nucleo_h755zi_q")
    set(DAS_MCU_BACKEND "cortex_m")
    set(DAS_DEVICE_BACKEND "stm32h755")
    set(DAS_BOARD_BACKEND "nucleo_h755zi_q")
    set(DAS_CMSIS_DEVICE_HEADER_NAME "stm32h755xx.h")

    if(DAS_CORE STREQUAL "cm7")
        set(DAS_CORE_HEADER "core_cm7.h")
        set(DAS_CORE_DEFINE "CORE_CM7")
        set(DAS_DEFAULT_LINKER_SCRIPT
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/targets/stm32h755_cm7.ld")
    elseif(DAS_CORE STREQUAL "cm4")
        set(DAS_CORE_HEADER "core_cm4.h")
        set(DAS_CORE_DEFINE "CORE_CM4")
        set(DAS_DEFAULT_LINKER_SCRIPT
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/targets/stm32h755_cm4.ld")
    else()
        message(FATAL_ERROR
            "Unsupported DAS_CORE='${DAS_CORE}' for ${DAS_DEVICE}; expected cm7 or cm4")
    endif()

    set(DAS_MCU_SOURCES
        src/mcu/cortex_m/startup.c
        src/mcu/cortex_m/irq.c)

    set(DAS_DEVICE_SOURCES
        src/device/stm32h755/gpio.c
        src/device/stm32h755/gpio_irq.c)

    set(DAS_BOARD_SOURCES
        src/board/nucleo_h755zi_q/board.c)
else()
    message(FATAL_ERROR "Unsupported DAS_DEVICE='${DAS_DEVICE}'")
endif()

if(DAS_LINKER_SCRIPT)
    get_filename_component(DAS_EFFECTIVE_LINKER_SCRIPT
        "${DAS_LINKER_SCRIPT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
else()
    set(DAS_EFFECTIVE_LINKER_SCRIPT "${DAS_DEFAULT_LINKER_SCRIPT}")
endif()

if(NOT EXISTS "${DAS_EFFECTIVE_LINKER_SCRIPT}")
    message(FATAL_ERROR "DAS linker script not found: ${DAS_EFFECTIVE_LINKER_SCRIPT}")
endif()

if(NOT STM32_CUBE_H7_DIR)
    message(FATAL_ERROR "STM32_CUBE_H7_DIR is required for ${DAS_DEVICE}")
endif()

set(_cmsis_core_candidates
    "${STM32_CUBE_H7_DIR}/Drivers/CMSIS/Core/Include"
    "${STM32_CUBE_H7_DIR}/Drivers/CMSIS/Include")
set(DAS_CMSIS_CORE_INCLUDE "")
foreach(_candidate IN LISTS _cmsis_core_candidates)
    if(EXISTS "${_candidate}/${DAS_CORE_HEADER}")
        set(DAS_CMSIS_CORE_INCLUDE "${_candidate}")
        break()
    endif()
endforeach()

set(DAS_CMSIS_DEVICE_INCLUDE
    "${STM32_CUBE_H7_DIR}/Drivers/CMSIS/Device/ST/STM32H7xx/Include")

if(NOT DAS_CMSIS_CORE_INCLUDE)
    message(FATAL_ERROR
        "CMSIS ${DAS_CORE_HEADER} not found under ${STM32_CUBE_H7_DIR}/Drivers/CMSIS")
endif()
if(NOT EXISTS "${DAS_CMSIS_DEVICE_INCLUDE}/${DAS_CMSIS_DEVICE_HEADER_NAME}")
    message(FATAL_ERROR
        "STM32H755 CMSIS device header not found under ${DAS_CMSIS_DEVICE_INCLUDE}")
endif()

message(STATUS
    "DAS composition: mcu=${DAS_MCU_BACKEND}, core=${DAS_CORE}, device=${DAS_DEVICE_BACKEND}, board=${DAS_BOARD_BACKEND}")
message(STATUS "DAS linker script: ${DAS_EFFECTIVE_LINKER_SCRIPT}")

add_library(das STATIC
    ${DAS_MCU_SOURCES}
    ${DAS_DEVICE_SOURCES}
    ${DAS_BOARD_SOURCES})
add_library(das::das ALIAS das)

target_include_directories(das
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "${DAS_CMSIS_CORE_INCLUDE}" "${DAS_CMSIS_DEVICE_INCLUDE}")
target_compile_definitions(das PRIVATE
    ${DAS_CORE_DEFINE}
    STM32H755xx
    DAS_CMSIS_DEVICE_HEADER=\"${DAS_CMSIS_DEVICE_HEADER_NAME}\")
target_compile_features(das PUBLIC c_std_11)
target_compile_options(das PRIVATE -Wall -Wextra -Werror)

# The linker script belongs to the final firmware ELF, not to libdas.a. Carry
# the selected device/core memory policy transitively so linking das::das is
# sufficient for the normal bare-metal case. DAS_LINKER_SCRIPT can override it.
target_link_options(das INTERFACE "-T${DAS_EFFECTIVE_LINKER_SCRIPT}")
set_property(TARGET das APPEND PROPERTY INTERFACE_LINK_DEPENDS
    "${DAS_EFFECTIVE_LINKER_SCRIPT}")

if(DAS_BUILD_LINK_TESTS)
    add_subdirectory(tests/link/stm32h755)
endif()

if(DAS_BUILD_HARDWARE_TESTS)
    add_subdirectory(tests/hardware/stm32h755)
endif()
