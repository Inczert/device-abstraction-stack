cmake_minimum_required(VERSION 3.20)
project(das_eth_raw LANGUAGES C)

find_package(DAS 0.1 CONFIG REQUIRED)

add_executable(das_eth_raw main.c)
target_link_libraries(das_eth_raw PRIVATE das::das)
target_compile_features(das_eth_raw PRIVATE c_std_11)
target_compile_options(das_eth_raw PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_eth_raw PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_eth_raw.map)
set_target_properties(das_eth_raw PROPERTIES SUFFIX ".elf")
