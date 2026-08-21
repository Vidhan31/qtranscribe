---
name: qtranscribe-build
description: >-
  Workflows and commands for building QTranscribe components separately
  (Qt GUI application and keyinjectord daemon). Use when the user asks to build,
  clean build, recompile, or configure the Qt GUI client or keyinjectord daemon
  independently (excluding unified build-all).
---

# QTranscribe Separate Component Build Guide

Build procedures and commands for compiling the Qt 6 GUI application and keyinjectord daemon independently.

## Why Build Separately?

1. **Preserve Daemon Capabilities:** `keyinjectord` requires elevated Linux capabilities (`cap_dac_override`) to open `/dev/uinput`. Whenever `keyinjectord` is re-linked, the kernel strips this capability. Keeping `keyinjectord` in its own build directory (`build-keyinjectord/`) means you do **not** have to re-run `sudo setcap` when editing and rebuilding the Qt GUI.
2. **Faster Iteration:** Developing UI/QML or core logic does not re-compile or re-link daemon code.
3. **Isolated Dependencies:** The Qt GUI requires Qt 6 & QtKeychain; `keyinjectord` requires only `libevdev` and `libcap`.
4. **Capability Provisioning Rule:** Agents must **never** execute `sudo` or `setcap` directly. Always instruct the **user** to run the `setcap` command when `keyinjectord` is re-linked.

---

## Build Reference Table

| Target | Operation | Build Dir | Build & Verify Command | Post-Build User Action |
| :--- | :--- | :--- | :--- | :--- |
| **Qt GUI** | Clean Debug | `build/` | `rm -rf build && cmake --preset linux-qt6-debug && cmake --build build && ctest --preset test-debug` | None |
| **Qt GUI** | Incremental | `build/` | `cmake --build build && ctest --preset test-debug` | None |
| **Qt GUI** | Clean Release | `build-release/` | `rm -rf build-release && cmake --preset linux-qt6-release -DBUILD_KEYINJECTORD=OFF && cmake --build build-release && ctest --preset test-release` | None |
| **keyinjectord** | Clean Debug | `build-keyinjectord/` | `rm -rf build-keyinjectord && cmake -S src/keyinjectord --preset keyinjectord-debug && cmake --build build-keyinjectord` | User runs: `sudo setcap "cap_dac_override+p" build-keyinjectord/keyinjectord` |
| **keyinjectord** | Incremental | `build-keyinjectord/` | `cmake --build build-keyinjectord` | If re-linked, user runs: `sudo setcap "cap_dac_override+p" build-keyinjectord/keyinjectord` |
| **keyinjectord** | Clean Release | `build-keyinjectord-release/` | `rm -rf build-keyinjectord-release && cmake -S src/keyinjectord --preset keyinjectord-release && cmake --build build-keyinjectord-release` | User runs: `sudo setcap "cap_dac_override+p" build-keyinjectord-release/keyinjectord` |
