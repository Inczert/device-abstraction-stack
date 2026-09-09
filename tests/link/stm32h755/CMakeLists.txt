add_executable(das_stm32h755_link_test firmware.c)
target_link_libraries(das_stm32h755_link_test PRIVATE das::das)
target_compile_features(das_stm32h755_link_test PRIVATE c_std_11)
target_compile_options(das_stm32h755_link_test PRIVATE
    -Wall -Wextra -Werror -ffreestanding -fno-builtin)
target_link_options(das_stm32h755_link_test PRIVATE
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--unresolved-symbols=report-all
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/das_stm32h755_link_test.map)
set_target_properties(das_stm32h755_link_test PROPERTIES SUFFIX ".elf")
