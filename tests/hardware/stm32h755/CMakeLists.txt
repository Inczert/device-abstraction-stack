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

# Global clock-tree changes are owned by CPU1/CM7. Keep the qualification image
# separate so the normal GPIO/IRQ campaign starts again from reset clock state.
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
endif()
