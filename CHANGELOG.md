# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.3.0] - 2026-11-17

### Added

- Added Best Fit spectrum allocation.
- Added Random Fit spectrum allocation using system entropy so that allocation order varies between simulation runs.
- Added Least Used and Most Used spectrum allocation based on global per-slot usage.
- Added topology and traffic-demand resources for Europe-20nodes, Germany-50nodes, Japan-12nodes, Spain-21nodes, UK-21nodes, and USA-14nodes.

### Changed

- Upgraded the bundled MONet simulator engine.
- Clarified the common physical-impairment-aware allocation behavior, including GN admission control, symmetric provisioning, and staged-connection rollback.
- Documented all available topology resources and spectrum-allocation alternatives.

## [0.2.1] - 2026-10-12

### Fixed

- Updated CI smoke-test commands to use the complete seven-argument command-line interface.
- Updated Docker documentation to reference the `0.2.1` image.

## [0.2.0] - 2026-10-12

### Added

- Added shortest-first, longest-first, least-loaded-first, and most-loaded-first route-ordering strategies.
- Added selectable all, low, and high bitrate sets.
- Added continuous-integration builds, simulator smoke tests, and Docker image validation.

### Changed

- Expanded the command-line interface to accept route ordering, bitrate-set selection, and spectrum-allocation selection.
- Reorganized command-line parsing and route-ordering utilities into shared configuration helpers.
- Improved local and Docker usage documentation for Linux, macOS, WSL, and Windows.

### Fixed

- Corrected command-line terminology to distinguish spectrum allocation from the complete RCSA process.

## [0.1.1] - 2026-09-18

### Fixed

- Added separate Docker instructions for Unix systems and Windows PowerShell.
- Updated Unix Docker commands to run with the current user's UID and GID, preventing root-owned result files.

## [0.1.0] - 2026-09-18

### Added

- Added the initial MONet-based command-line simulator.
- Added physical-impairment-aware First Fit spectrum allocation with GN admission control.
- Added shortest-path and disjoint-path route-set selection.
- Added the Germany-14nodes topology, traffic demands, bitrate definitions, and SSMF fiber specification.
- Added CMake and Docker build configurations.
- Added automatic multi-platform Docker image publishing through GitHub Actions and GitHub Container Registry.
- Added TopoLib-based topology and traffic-demand resource generation.
- Added CSV simulation reports and configurable output directories.

[Unreleased]: https://github.com/MirkoZETA/monet-rcsa-exercise-tool/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/MirkoZETA/monet-rcsa-exercise-tool/compare/v0.2.1...v0.3.0
[0.2.1]: https://github.com/MirkoZETA/monet-rcsa-exercise-tool/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/MirkoZETA/monet-rcsa-exercise-tool/compare/v0.1.1...v0.2.0
[0.1.1]: https://github.com/MirkoZETA/monet-rcsa-exercise-tool/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/MirkoZETA/monet-rcsa-exercise-tool/releases/tag/v0.1.0
