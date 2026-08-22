---
name: qtranscribe-build
description: >-
  Workflows and commands for building QTranscribe components separately
  (Qt GUI application and keyinjectord daemon). Use when the user asks to build,
  clean build, recompile, or configure the Qt GUI client or keyinjectord daemon
  independently (excluding unified build-all).
---

# QTranscribe Build Guide

Build procedures and commands for compiling QTranscribe and its helper daemon `keyinjectord`.

## Security & Capability Rules

1. **Development Environment:** `keyinjectord` runs unprivileged in local development builds. Access to `/dev/uinput` is provided via membership in the `input` group (`/etc/udev/rules.d/99-uinput.rules`).
2. **Capability Mode (`cap_dac_override`):** In production packaging (.deb, .rpm, Arch), `keyinjectord` is installed as root into `/usr/bin` or `/opt/qtranscribe/bin` with `cap_dac_override=p`. `keyinjectord` strictly refuses capability-bearing execution in user-owned build directories to prevent launcher forgery.
3. **Colocation:** All binaries (`qtranscribe` and `keyinjectord`) are built and installed into the same directory (`build/` during development, or `/usr/bin` / `/opt/qtranscribe/bin` when packaged).

---

## Build Reference Table

| Target | Operation | Build Dir | Build & Verify Command |
| :--- | :--- | :--- | :--- |
| **Unified App + Daemon** | Clean Debug | `build/` | `rm -rf build && cmake --preset linux-qt6-debug && cmake --build build && ctest --preset test-debug` |
| **Unified App + Daemon** | Incremental | `build/` | `cmake --build build && ctest --preset test-debug` |
| **Unified App + Daemon** | Clean Release | `build-release/` | `rm -rf build-release && cmake --preset linux-qt6-release && cmake --build build-release && ctest --preset test-release` |
