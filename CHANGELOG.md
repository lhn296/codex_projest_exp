# Changelog

All notable changes to this project will be documented in this file.

## v2.1.0 - 2026-03-20

### Added
- Added LCD config-source visualization so the project info area can display `CFG : DEFAULT / NVS / MIXED / RUNTIME`.
- Added CLI enhancement commands: `cfg status`, `cfg source`, `cfg test http`, and `cfg test ota`.
- Added `ota_service_check_now()` so OTA version checking can be triggered manually from the CLI.

### Changed
- Changed the project positioning from device configuration foundation to configuration visualization and CLI enhancement learning.
- Changed `display_service` project info layout so version, stage, and config source can be shown together.
- Changed `config_cli_service` so configuration changes, loads, and resets also refresh the LCD config-source summary.

## v2.0.0 - 2026-03-20

### Added
- Added `config_service` with runtime config structure, default-value loading, NVS save/load, reset-to-default, and self-test flow.
- Added `config_cli_service` so configuration can be viewed and modified through `idf.py monitor` with `cfg ...` commands.
- Added bad-URL protection, including basic URL validation before save and fallback-to-default on invalid NVS URL values.

### Changed
- Changed the project positioning from real OTA upgrade learning to device configuration foundation learning.
- Changed `wifi_service`, `http_service`, and `ota_service` to read key runtime parameters from `config_service` instead of directly from fixed macros.
- Changed OTA version comparison from simple string inequality to semantic numeric comparison for `vX.Y.Z`.

## v1.9.0 - 2026-03-19

### Added
- Added real OTA download-and-apply flow in `ota_service`, including `esp_ota_begin`, chunked `esp_ota_write`, `esp_ota_end`, boot-partition switch, and reboot.
- Added `OTA_STATE_VERIFY` so the OTA state machine can express the post-download verification and switch stage.
- Added `APP_OTA_WRITE_BUFFER_SIZE` in `app_config.h` for OTA write-buffer control.

### Changed
- Changed the project positioning from cloud version-check learning to real OTA upgrade learning.
- Changed the partition layout to `Two OTA Large`, giving the project dual OTA app slots and practical space for real upgrades.
- Updated project metadata and LCD OTA status display to describe the `v1.9.0` real OTA upgrade stage.

## v1.8.0 - 2026-03-18

### Added
- Added real cloud-version-check preparation in `ota_service`, including parsing of `version / url / message` from a remote JSON payload.
- Added `http_service_get_response_body()` so upper-layer modules such as `ota_service` can consume the full HTTP response body.
- Added HTTPS certificate bundle attachment for HTTP requests, preparing the project for services such as `Cloudflare Workers`.

### Changed
- Changed the project positioning from local OTA skeleton learning to cloud OTA version-check learning.
- Changed OTA metadata flow from local compile-time mock values to HTTP-driven cloud payload parsing.
- Updated project metadata and docs to describe the `v1.8.0` cloud version check stage.

## v1.6.0 - 2026-03-18

### Added
- Added `http_service` as a reusable HTTP GET and JSON parsing foundation layer.
- Added LCD HTTP result display support for `HTTP / CODE / MSG`.
- Added HTTP configuration entries in `app_config.h` for test URL, timeout, and auto-start behavior.

### Changed
- Changed project positioning from Wi-Fi-only connectivity learning to HTTP and JSON application-layer learning.
- Changed the LCD home page so it can now display HTTP result status together with Wi-Fi information.
- Updated startup logs and metadata to describe the `v1.6.0` HTTP JSON foundation stage.

## v1.5.0 - 2026-03-16

### Added
- Added `wifi_service` as a reusable Wi-Fi STA foundation layer with event-driven state updates.
- Added Wi-Fi state and IP display support in `display_service`.
- Added Wi-Fi configuration entries in `app_config.h` for SSID, password, retry count, and timeout.

### Changed
- Changed project positioning from display-only optimization to Wi-Fi network foundation learning.
- Changed the LCD home page so it can now display Wi-Fi state and current IP address.
- Updated startup logs and metadata to describe the `v1.5.0` Wi-Fi network foundation stage.

## v1.4.1 - 2026-03-16

### Added
- Added LCD local-region clear helper in `bsp_lcd` for later partial refresh support.
- Added `v1.4.1` display-layout planning docs for the display-service optimization stage.

### Changed
- Changed `display_service` from single-page dirty redraw to region-based dirty refresh.
- Changed the LCD home page into clearer header, project info, LED, beep, and last-event sections.
- Updated project metadata and startup stage description to `v1.4.1` display-service optimization.

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
