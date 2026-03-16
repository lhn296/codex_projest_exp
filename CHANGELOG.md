# Changelog

All notable changes to this project will be documented in this file.

## v1.4.0 - 2026-03-16

### Added
- Added reusable `spi_bus` driver helpers for SPI bus init, device registration, and raw transmit wrappers.
- Added reusable `lcd_st7789v` display driver with init, fill, and basic text drawing helpers.
- Added `bsp_lcd` board-adaptation layer to combine `XL9555` power/reset control with LCD SPI setup.
- Added `display_service` to show version, stage, LED status, buzzer status, and last button event on the LCD home page.

### Changed
- Changed project positioning from board interaction learning to SPI LCD display template learning.
- Changed the unified event task so button events now refresh LCD state together with LED and buzzer feedback.
- Updated startup logs and metadata to describe the `v1.4.0` SPI LCD display template stage.

## v1.3.1 - 2026-03-16

### Added
- Added `BTN_FUNC` so all four board keys can participate in the unified button event chain.
- Added `beep_service` for lightweight non-blocking buzzer patterns and test mode.
- Added buzzer timing and active-level configuration entries in `app_config.h`.

### Changed
- Changed board `KEY3` from reserved input to functional key for buzzer enable/test control.
- Changed `app_event_task` so business keys drive `LED + BEEP`, while the function key controls buzzer behavior.
- Updated project metadata and logs to describe the `v1.3.1` board interaction stage.

## v1.3.0 - 2026-03-15

### Added
- Added a reusable `i2c_bus` access layer for bus init, device probe, and register read/write helpers.
- Added a reusable `xl9555` driver layer with port direction, pin read/write, and bit operation helpers.
- Added a `bsp_xl9555` board-adaptation layer for board keys, beep, LCD control pins, and shared INT setup.

### Changed
- Changed button input from direct ESP32 GPIO to board `XL9555` based key input with shared INT wake-up.
- Changed `APP_SYS_LED_GPIO` to `GPIO16` so `GPIO0` can be reserved for `XL9555 INT`.
- Updated project metadata and startup logs to describe the `v1.3.0` XL9555 driver foundation stage.

## v1.2.1 - 2026-03-14

### Added
- Added unified event message definitions with source/type/param fields for future module expansion.
- Added basic queue send/receive/handle counters and clearer unified event logs.

### Changed
- Changed the queue payload from button-specific messages to a unified event message format.
- Kept button gesture behavior unchanged while moving the event chain toward a more reusable architecture.
- Updated project metadata and logs to describe the `v1.2.1` unified event message stage.

## v1.2.0 - 2026-03-14

### Added
- Added `FreeRTOS Queue` based button event delivery with `app_button_msg_t`.
- Added `app_event_task` to receive button events and handle LED business actions.
- Added queue/task configuration entries for the first event-driven architecture version.

### Changed
- Changed `button_service` from direct LED control to queue-based event publishing.
- Kept the GPIO negative-edge interrupt trigger and gesture behavior while moving business handling into a dedicated event task.
- Updated project metadata and logs to describe the `v1.2.0` queue event architecture stage.

## v1.1.2 - 2026-03-14

### Added
- Added independent GPIO negative-edge interrupt handling for `BTN_SYS`, `BTN_NET`, and `BTN_ERR`.
- Added button IRQ pending consumption API in `bsp_button` for interrupt-driven button processing.

### Changed
- Changed button handling from pure periodic polling to a hybrid model: interrupt notification plus main-loop state-machine processing.
- Kept debounce, short press, long press, and double click behavior unchanged while moving the trigger source to GPIO interrupts.
- Updated project metadata and startup logs to describe the `v1.1.2` negative-edge interrupt learning stage.

## v1.1.1 - 2026-03-14

### Added
- Added button debounce timing and gesture timing configuration in `app_config.h`.
- Added button gesture recognition for short press, long press, and double click.

### Changed
- Short press now cycles LED modes as before.
- Long press now forces the corresponding LED to `OFF` from any current state.
- Double click now forces the corresponding LED to `BLINK_FAST` from any current state.
- Enhanced button logs to show the detected gesture and resulting LED mode transition.

## v1.1.0 - 2026-03-14

### Changed
- Positioned the project as the external GPIO input/output learning version for the first hands-on stage.
- Updated startup logs to print the current learning stage and the active button/LED GPIO mapping.
- Improved button service logs so each button press clearly shows the target LED and mode transition.

### Documentation
- Refreshed project metadata and docs to match the `v1.1.0` learning milestone.

## v1.0.0 - 2026-03-13

### Added
- Established this repository as the initial ESP32-S3 template baseline for follow-up module learning and expansion.
- Included a layered project structure covering `main`, `app`, `services`, `bsp`, and `system`.
- Included three-button, three-LED interaction flow with mode cycling for `OFF`, `ON`, `BLINK_SLOW`, and `BLINK_FAST`.

### Changed
- Centralized project metadata, target information, GPIO assignments, default LED modes, and task settings in `app_config.h`.
- Aligned startup output and project documentation with the new `v1.0.0` baseline.
- Updated button handling logic to use centralized button count and active-level configuration.

### Fixed
- Corrected LED mode wraparound so `LED_MODE_BLINK_FAST` participates in the full mode cycle.
