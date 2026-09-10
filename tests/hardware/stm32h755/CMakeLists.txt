add_executable(das_stm32h755_hw_test firmware.c)
target_link_libraries(das_stm32h755_hw_test PRIVATE das::das)
target_include_directories(das_stm32h755_hw_test PRIVATE
    "${DAS_CMSIS_CORE_INCLUDE}"
    "${DAS_CMSIS_DEVICE_INCLUDE}")
target_compile_definitions(das_stm32h755_hw_test PRIVATE ${DAS_CORE_DEFINE} STM32H755xx)
target_compile_features(das_stm32h755_hw_test PRIVATE c_std_11)
target_compile_options(das_stm32h755_hw_test PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_stm32h755_hw_test PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_stm32h755_hw_test.map)
set_target_properties(das_stm32h755_hw_test PROPERTIES SUFFIX ".elf")

add_executable(das_stm32h755_time_test time_firmware.c)
target_link_libraries(das_stm32h755_time_test PRIVATE das::das)
target_include_directories(das_stm32h755_time_test PRIVATE
    "${DAS_CMSIS_CORE_INCLUDE}"
    "${DAS_CMSIS_DEVICE_INCLUDE}")
target_compile_definitions(das_stm32h755_time_test PRIVATE ${DAS_CORE_DEFINE} STM32H755xx)
target_compile_features(das_stm32h755_time_test PRIVATE c_std_11)
target_compile_options(das_stm32h755_time_test PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_stm32h755_time_test PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_stm32h755_time_test.map)
set_target_properties(das_stm32h755_time_test PROPERTIES SUFFIX ".elf")
add_dependencies(das_stm32h755_hw_test das_stm32h755_time_test)

add_executable(das_stm32h755_uart_test uart_firmware.c)
target_link_libraries(das_stm32h755_uart_test PRIVATE das::das)
target_include_directories(das_stm32h755_uart_test PRIVATE
    "${DAS_CMSIS_CORE_INCLUDE}"
    "${DAS_CMSIS_DEVICE_INCLUDE}")
target_compile_definitions(das_stm32h755_uart_test PRIVATE ${DAS_CORE_DEFINE} STM32H755xx)
target_compile_features(das_stm32h755_uart_test PRIVATE c_std_11)
target_compile_options(das_stm32h755_uart_test PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_stm32h755_uart_test PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_stm32h755_uart_test.map)
set_target_properties(das_stm32h755_uart_test PROPERTIES SUFFIX ".elf")
add_dependencies(das_stm32h755_hw_test das_stm32h755_uart_test)

add_executable(das_stm32h755_timer_test timer_firmware.c)
target_link_libraries(das_stm32h755_timer_test PRIVATE das::das)
target_include_directories(das_stm32h755_timer_test PRIVATE
    "${DAS_CMSIS_CORE_INCLUDE}"
    "${DAS_CMSIS_DEVICE_INCLUDE}")
target_compile_definitions(das_stm32h755_timer_test PRIVATE ${DAS_CORE_DEFINE} STM32H755xx)
target_compile_features(das_stm32h755_timer_test PRIVATE c_std_11)
target_compile_options(das_stm32h755_timer_test PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_stm32h755_timer_test PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_stm32h755_timer_test.map)
set_target_properties(das_stm32h755_timer_test PROPERTIES SUFFIX ".elf")
add_dependencies(das_stm32h755_hw_test das_stm32h755_timer_test)

add_executable(das_stm32h755_spi_test spi_firmware.c)
target_link_libraries(das_stm32h755_spi_test PRIVATE das::das)
target_include_directories(das_stm32h755_spi_test PRIVATE
    "${DAS_CMSIS_CORE_INCLUDE}"
    "${DAS_CMSIS_DEVICE_INCLUDE}")
target_compile_definitions(das_stm32h755_spi_test PRIVATE ${DAS_CORE_DEFINE} STM32H755xx)
target_compile_features(das_stm32h755_spi_test PRIVATE c_std_11)
target_compile_options(das_stm32h755_spi_test PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_stm32h755_spi_test PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_stm32h755_spi_test.map)
set_target_properties(das_stm32h755_spi_test PROPERTIES SUFFIX ".elf")
add_dependencies(das_stm32h755_hw_test das_stm32h755_spi_test)

add_executable(das_stm32h755_i2c_test i2c_firmware.c)
target_link_libraries(das_stm32h755_i2c_test PRIVATE das::das)
target_include_directories(das_stm32h755_i2c_test PRIVATE
    "${DAS_CMSIS_CORE_INCLUDE}"
    "${DAS_CMSIS_DEVICE_INCLUDE}")
target_compile_definitions(das_stm32h755_i2c_test PRIVATE ${DAS_CORE_DEFINE} STM32H755xx)
target_compile_features(das_stm32h755_i2c_test PRIVATE c_std_11)
target_compile_options(das_stm32h755_i2c_test PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_stm32h755_i2c_test PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_stm32h755_i2c_test.map)
set_target_properties(das_stm32h755_i2c_test PROPERTIES SUFFIX ".elf")
add_dependencies(das_stm32h755_hw_test das_stm32h755_i2c_test)

add_executable(das_stm32h755_dma_test dma_firmware.c)
target_link_libraries(das_stm32h755_dma_test PRIVATE das::das)
target_include_directories(das_stm32h755_dma_test PRIVATE
    "${DAS_CMSIS_CORE_INCLUDE}"
    "${DAS_CMSIS_DEVICE_INCLUDE}")
target_compile_definitions(das_stm32h755_dma_test PRIVATE ${DAS_CORE_DEFINE} STM32H755xx)
target_compile_features(das_stm32h755_dma_test PRIVATE c_std_11)
target_compile_options(das_stm32h755_dma_test PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_stm32h755_dma_test PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_stm32h755_dma_test.map)
set_target_properties(das_stm32h755_dma_test PROPERTIES SUFFIX ".elf")
add_dependencies(das_stm32h755_hw_test das_stm32h755_dma_test)

# Global clock-tree changes and the board-resource/button qualification are
# owned by CPU1/CM7. Keep these images separate from the reusable GPIO image.
if(DAS_CORE STREQUAL "cm7")
    add_executable(das_stm32h755_clock_test clock_firmware.c)
    target_link_libraries(das_stm32h755_clock_test PRIVATE das::das)
    target_include_directories(das_stm32h755_clock_test PRIVATE
        "${DAS_CMSIS_CORE_INCLUDE}"
        "${DAS_CMSIS_DEVICE_INCLUDE}"
        "${CMAKE_SOURCE_DIR}/src/device/stm32h755")
    target_compile_definitions(das_stm32h755_clock_test PRIVATE ${DAS_CORE_DEFINE} STM32H755xx)
    target_compile_features(das_stm32h755_clock_test PRIVATE c_std_11)
    target_compile_options(das_stm32h755_clock_test PRIVATE
        -Wall -Wextra -Werror -ffreestanding -fno-builtin)
    target_link_options(das_stm32h755_clock_test PRIVATE
        -nostartfiles
        --specs=nosys.specs
        -Wl,--gc-sections
        -Wl,--unresolved-symbols=report-all
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_stm32h755_clock_test.map)
    set_target_properties(das_stm32h755_clock_test PROPERTIES SUFFIX ".elf")
    add_dependencies(das_stm32h755_hw_test das_stm32h755_clock_test)

    add_executable(das_stm32h755_button_test button_firmware.c)
    target_link_libraries(das_stm32h755_button_test PRIVATE das::das)
    target_include_directories(das_stm32h755_button_test PRIVATE
        "${DAS_CMSIS_CORE_INCLUDE}"
        "${DAS_CMSIS_DEVICE_INCLUDE}")
    target_compile_definitions(das_stm32h755_button_test PRIVATE ${DAS_CORE_DEFINE} STM32H755xx)
    target_compile_features(das_stm32h755_button_test PRIVATE c_std_11)
    target_compile_options(das_stm32h755_button_test PRIVATE
        -Wall -Wextra -Werror -ffreestanding -fno-builtin)
    target_link_options(das_stm32h755_button_test PRIVATE
        -nostartfiles
        --specs=nosys.specs
        -Wl,--gc-sections
        -Wl,--unresolved-symbols=report-all
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_stm32h755_button_test.map)
    set_target_properties(das_stm32h755_button_test PROPERTIES SUFFIX ".elf")
    add_dependencies(das_stm32h755_hw_test das_stm32h755_button_test)
endif()
