# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.1.2] - 2020-04-16
### Changed
- Updated the docs
- Read (and in debug mode print out) sensor values even without an active wifi connection

## [2.1.1] - 2020-04-15
### Changed
- Fixed MQTT topic to include the whole OBIS identifier
- Look for a reading head attached to GPIO pin D2 by default
- Switched to official libSML repository