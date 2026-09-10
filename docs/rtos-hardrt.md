# HardRT integration

DAS and HardRT can be used together on STM32H755 without giving both libraries ownership of the same Cortex-M facilities. The current DAS reference integration targets **HardRT 0.5.1 or a compatible newer 0.5.x release**. HardRT 0.5.1 is the validated baseline used for the current Cortex-M scheduler/ISR, diagnostics, IPC, event/notification, and CMake package integration.

The reference model is:

```text
DAS                         HardRT
---                         ------
RCC/PWR/FLASH clocking      scheduler
GPIO/UART/SPI/I2C/DMA       SysTick tick
board resources             PendSV context switching
linker/memory policy        HardFault diagnostics
weak reset/core handlers    tasks/sleep/IPC
```

The reference application is `examples/hardrt_uart`.

## Exception ownership

The current HardRT Cortex-M port provides strong `SysTick_Handler`, `PendSV_Handler` and diagnostic `HardFault_Handler` implementations. DAS provides weak core-handler defaults and a weak default STM32H755 vector table. The final link therefore keeps the DAS vector layout while the strong HardRT handlers replace the weak DAS handlers referenced by those core vector slots.

The HardRT reference application links `HardRT::hardrt` before `das::das` and explicitly requests `HardFault_Handler`, `PendSV_Handler` and `SysTick_Handler` from the static archives. This matters because both dependencies are static libraries and the linker otherwise has no obligation to extract an object merely because it contains a stronger definition than an already satisfiable weak symbol.

HardRT does not currently require SVC for its scheduler path. DAS's weak SVC default therefore remains active. If HardRT later gains SVC ownership, the same rule must be extended deliberately rather than allowing two implementations to coexist accidentally.

## Clock initialization order

Set the DAS board clock **before** HardRT and before clock-dependent peripheral initialization:

```c
uint32_t core_hz = 0u;

das_clock_set_frequency(400000000u);
das_clock_get_core_frequency(&core_hz);

hrt_config_t cfg = {
    .tick_hz = 1000u,
    .policy = HRT_SCHED_PRIORITY_RR,
    .default_slice = 5u,
    .core_hz = core_hz,
    .tick_src = HRT_TICK_SYSTICK,
};

hrt_init(&cfg);
```

HardRT uses `core_hz` to calculate the SysTick reload. The reference application also overrides HardRT's weak `hrt_port_get_core_hz()` fallback to return the DAS-observed core frequency, removing any dependency on vendor `SystemCoreClock` initialization.

Changing the CPU/system frequency after HardRT has configured SysTick changes the real tick period. Changing it after UART/SPI/I2C/timer initialization can likewise invalidate peripheral timing. Runtime clock changes therefore require coordinated reconfiguration and are not treated as a transparent operation.

## DAS time under HardRT

Do **not** call `das_time_init()` when HardRT owns SysTick. `das_time_init()` configures SysTick for the standalone DAS millisecond backend and would overwrite the RTOS tick setup.

Instead expose HardRT's monotonic milliseconds to DAS:

```c
static das_time_ms_t hardrt_now(void* context)
{
    (void)context;
    return hrt_now_ms();
}

/* after successful hrt_init() */
das_time_set_source(hardrt_now, 0);
```

This lets DAS finite-time UART/SPI/I2C operations use the RTOS clock while HardRT remains the sole SysTick owner.

`das_delay_ms()` remains a busy wait even with this source and should normally not be used inside RTOS tasks. Use `hrt_sleep()` for task delays.

## UART and other peripheral ownership

DAS peripheral drivers remain usable from HardRT tasks. A typical setup sequence is:

```text
DAS clock profile
    -> DAS board/peripheral initialization
    -> HardRT initialization
    -> DAS time source = hrt_now_ms()
    -> create tasks
    -> hrt_start()
```

The current DAS UART API is synchronous and has no built-in RTOS mutex. One task should own a UART, or callers should serialize access using a HardRT mutex/central I/O task. The same rule applies to any stateful peripheral that multiple tasks might access concurrently.

For future interrupt-driven DAS APIs that call HardRT services from an ISR, interrupt priority must respect HardRT's Cortex-M `BASEPRI` / maximum-syscall-priority contract.

## Memory/linker contract

HardRT validates task stacks against linker symbols `__RAM_START__` and `__RAM_END__`. DAS default STM32H755 linker scripts export these aliases for their core-specific writable regions:

```text
CM7 -> AXI SRAM
CM4 -> D2 SRAM1
```

This allows a HardRT application to use the DAS linker/startup policy rather than carrying the old HardRT STM32 example linker script merely to satisfy stack validation.

Custom linker scripts used with HardRT must provide equivalent RAM bounds.

## CMake

Both libraries are consumed as installed packages:

```cmake
set(DAS_USE_DEFAULT_VECTOR_TABLE ON)
find_package(DAS CONFIG REQUIRED)
find_package(HardRT 0.5.1 CONFIG REQUIRED)

target_link_libraries(app PRIVATE HardRT::hardrt das::das)
target_link_options(app PRIVATE
    -Wl,-u,HardFault_Handler
    -Wl,-u,PendSV_Handler
    -Wl,-u,SysTick_Handler)
```

The DAS default vector table stays useful here: the application does not need to copy a full STM32 vector table just to let HardRT replace the core handlers it owns.

CI cross-builds the reference HardRT+DAS CM7 application and checks the final ELF for HardRT's strong scheduler/fault handlers, the DAS vector section and RAM-boundary symbols.
