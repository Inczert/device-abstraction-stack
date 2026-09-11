# Periodic time example

This is a standalone installed-package consumer. It resolves DAS with `find_package(DAS CONFIG REQUIRED)` and links only `das::das`.

The application selects the 200 MHz board clock profile, initializes the default DAS SysTick time source, and toggles the green user LED every 500 ms using wrap-safe monotonic-time helpers.

It is built automatically by the CI installed-consumer matrix for CM7 and CM4 packages.
