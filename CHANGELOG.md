# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Security
- Hardened `keyinjectord` launcher authorization to strictly enforce root ownership for production binaries and directories, preventing user-space launcher forgery.
- Restricted test executables to development builds via compile-time gating (`KEYINJECTORD_DEV_AUTH`).

### Removed
- Removed standalone portable tarball packaging (`.tar.gz`) to eliminate unprivileged capability vulnerability vectors and ensure distribution exclusively via system package managers (`.deb`, `.rpm`, Arch).

## [1.1.0] - 2026-08-22

### Added
- Embedded `whisper.cpp` engine for high-performance offline speech-to-text inference.
- Vulkan GPU acceleration with automatic CPU fallback and CMake ccache integration.
- Build-time unit test verification in CI and local build routines.

### Changed
- Optimized IPC protocol between QTranscribe and `keyinjectord` from JSON to a compact binary protocol.
- Refactored core STT audio pipeline and engine lifecycle management.
- Updated packaging definitions to build and bundle `whisper.cpp` dependencies across Debian, Arch, RPM, and tarball targets.

### Security
- Hardened `keyinjectord` IPC server with strict max buffer bounds and client connection limits to prevent DoS / buffer overflow.

### Fixed
- Fixed desktop portal integration and system tray icon assets.

## [1.0.0] - 2026-08-20

### Added
- Initial public release of QTranscribe.
- Pre-built distribution packages for Debian/Ubuntu (`.deb`), Fedora (`.rpm`), Arch Linux (`.pkg.tar.zst`) and standalone tarballs (`.tar.gz`).

[Unreleased]: https://github.com/Vidhan31/qtranscribe/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/Vidhan31/qtranscribe/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/Vidhan31/qtranscribe/releases/tag/v1.0.0
