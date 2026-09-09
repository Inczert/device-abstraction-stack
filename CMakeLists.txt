cmake_minimum_required(VERSION 3.20)
project(das VERSION 0.1.0 LANGUAGES C ASM)

set(DAS_DEVICE "nucleo_h755zi_q" CACHE STRING "Target DAS device")
set_property(CACHE DAS_DEVICE PROPERTY STRINGS nucleo_h755zi_q)
set(STM32_CUBE_H7_DIR "" CACHE PATH "Path to STM32CubeH7 checkout")
option(DAS_BUILD_HARDWARE_TESTS "Build physical board test firmware" OFF)

if(NOT DAS_DEVICE STREQUAL "nucleo_h755zi_q")
    message(FATAL_ERROR "Unsupported DAS_DEVICE='${DAS_DEVICE}'")
endif()

if(NOT STM32_CUBE_H7_DIR)
    message(FATAL_ERROR "STM32_CUBE_H7_DIR is required for ${DAS_DEVICE}")
endif()

set(_cmsis_core_candidates
    "${STM32_CUBE_H7_DIR}/Drivers/CMSIS/Core/Include"
    "${STM32_CUBE_H7_DIR}/Drivers/CMSIS/Include")
set(DAS_CMSIS_CORE_INCLUDE "")
foreach(_candidate IN LISTS _cmsis_core_candidates)
    if(EXISTS "${_candidate}/core_cm7.h")
        set(DAS_CMSIS_CORE_INCLUDE "${_candidate}")
        break()
    endif()
endforeach()

set(DAS_CMSIS_DEVICE_INCLUDE
    "${STM32_CUBE_H7_DIR}/Drivers/CMSIS/Device/ST/STM32H7xx/Include")

if(NOT DAS_CMSIS_CORE_INCLUDE)
    message(FATAL_ERROR "CMSIS core_cm7.h not found under ${STM32_CUBE_H7_DIR}/Drivers/CMSIS")
endif()
if(NOT EXISTS "${DAS_CMSIS_DEVICE_INCLUDE}/stm32h755xx.h")
    message(FATAL_ERROR "STM32H755 CMSIS device header not found under ${DAS_CMSIS_DEVICE_INCLUDE}")
endif()

add_library(das STATIC
    src/mcu/stm32h7/gpio.c
    src/device/nucleo_h755zi_q/board.c)
add_library(das::das ALIAS das)

target_include_directories(das
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "${DAS_CMSIS_CORE_INCLUDE}" "${DAS_CMSIS_DEVICE_INCLUDE}")
target_compile_definitions(das PRIVATE CORE_CM7 STM32H755xx)
target_compile_features(das PUBLIC c_std_11)
target_compile_options(das PRIVATE -Wall -Wextra -Werror)

if(DAS_BUILD_HARDWARE_TESTS)
    add_subdirectory(tests/hardware/stm32h755)
endif()
