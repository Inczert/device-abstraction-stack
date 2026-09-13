cmake_minimum_required(VERSION 3.20)
project(das_time_periodic LANGUAGES C)

find_package(DAS 0.1 CONFIG REQUIRED)

add_executable(das_time_periodic main.c)
target_link_libraries(das_time_periodic PRIVATE das::das)
target_compile_features(das_time_periodic PRIVATE c_std_11)
target_compile_options(das_time_periodic PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_time_periodic PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_time_periodic.map)
set_target_properties(das_time_periodic PROPERTIES SUFFIX ".elf")
