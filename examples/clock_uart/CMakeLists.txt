cmake_minimum_required(VERSION 3.20)
project(das_clock_uart LANGUAGES C)

find_package(DAS 0.1 CONFIG REQUIRED)

add_executable(das_clock_uart main.c)
target_link_libraries(das_clock_uart PRIVATE das::das)
target_compile_features(das_clock_uart PRIVATE c_std_11)
target_compile_options(das_clock_uart PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_clock_uart PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_clock_uart.map)
set_target_properties(das_clock_uart PROPERTIES SUFFIX ".elf")
