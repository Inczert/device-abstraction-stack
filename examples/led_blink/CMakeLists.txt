cmake_minimum_required(VERSION 3.20)
project(das_led_blink LANGUAGES C)

find_package(DAS 0.1 CONFIG REQUIRED)

add_executable(das_led_blink main.c)
target_link_libraries(das_led_blink PRIVATE das::das)
target_compile_features(das_led_blink PRIVATE c_std_11)
target_compile_options(das_led_blink PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_led_blink PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_led_blink.map)
set_target_properties(das_led_blink PROPERTIES SUFFIX ".elf")

message(STATUS "LED blink consumes DAS package: device=${DAS_PACKAGE_DEVICE}, core=${DAS_PACKAGE_CORE}")
