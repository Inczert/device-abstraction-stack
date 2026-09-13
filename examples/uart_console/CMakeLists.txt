cmake_minimum_required(VERSION 3.20)
project(das_uart_console LANGUAGES C)

find_package(DAS 0.1 CONFIG REQUIRED)

add_executable(das_uart_console main.c)
target_link_libraries(das_uart_console PRIVATE das::das)
target_compile_features(das_uart_console PRIVATE c_std_11)
target_compile_options(das_uart_console PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_uart_console PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_uart_console.map)
set_target_properties(das_uart_console PROPERTIES SUFFIX ".elf")
