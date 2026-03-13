# Changelog

All notable changes to this project will be documented in this file.

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
