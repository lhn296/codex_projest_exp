# Changelog

All notable changes to this project will be documented in this file.

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
