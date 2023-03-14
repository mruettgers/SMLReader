# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.3.0] - 2023-03-14
### Changed
- Upgraded IotWebConf to version 3
### Fixed
- [Issue #47](https://github.com/mruettgers/SMLReader/issues/47), Broken MQTT reconnect after WiFi disconnect

## [2.2.1] - 2022-03-17
### Changed
- [Issue #19](https://github.com/mruettgers/SMLReader/issues/19)
### Fixed
- The max length of the WiFi password has been increased to 64 chars

## [2.2.0] - 2021-05-04
### Changed
- Replaced MQTT client library with [Pangolin MQTT Client](https://github.com/philbowles/PangolinMQTT)
- Refactored throttling logic
### Fixed
- [Issue #15](https://github.com/mruettgers/SMLReader/issues/15)

## [2.1.6] - 2021-01-03
### Added
- Configuration option to allow setting an inverval for throttling the MQTT messages
### Fixed
- Locked third party dependency IotWebConf to version 2 due to incompatible changes in version 3

## [2.1.5] - 2020-07-25
### Changed
- Modified MQTT topic to use / instead of * due to invalid character issues when used with ioBroker

## [2.1.4] - 2020-07-24
### Changed
- Worked on the docs
- Added LED based feedback for SML data recognition

## [2.1.3] - 2020-04-17
### Changed
- Added hint to change AP password to the docs
- Moved WiFi default settings to `config.h`
### Fixed
- String initialization issues resulting in random MQTT username/passqord in UI

## [2.1.2] - 2020-04-16
### Changed
- Updated the docs
- Read (and in debug mode print out) sensor values even without an active wifi connection

## [2.1.1] - 2020-04-15
### Changed
- Fixed MQTT topic to include the whole OBIS identifier
- Look for a reading head attached to GPIO pin D2 by default
- Switched to official libSML repository
