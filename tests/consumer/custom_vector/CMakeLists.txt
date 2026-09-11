cmake_minimum_required(VERSION 3.20)
project(das_custom_vector_consumer LANGUAGES C)

option(DAS_CUSTOM_VECTOR_DISABLE_DEFAULT
    "Disable DAS default-vector injection before finding the package" ON)
if(DAS_CUSTOM_VECTOR_DISABLE_DEFAULT)
    set(DAS_USE_DEFAULT_VECTOR_TABLE OFF)
endif()

find_package(DAS 0.1 CONFIG REQUIRED)

add_executable(das_custom_vector_consumer main.c)
target_link_libraries(das_custom_vector_consumer PRIVATE das::das)
target_compile_features(das_custom_vector_consumer PRIVATE c_std_11)
target_compile_options(das_custom_vector_consumer PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_custom_vector_consumer PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_custom_vector_consumer.map)
set_target_properties(das_custom_vector_consumer PROPERTIES SUFFIX ".elf")
