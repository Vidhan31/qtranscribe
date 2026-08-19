---
name: qtranscribe-lint-format
description: >-
  Workflows, commands, and tools for formatting source code (C++, QML) and running
  linters (qmllint, pre-commit checks) across the QTranscribe project. Use when
  formatting code, resolving lint warnings, checking style conformity, or running
  pre-commit hooks before committing changes.
---

# QTranscribe Code Formatting & Linting Guide

Commands and workflows for formatting C++ and QML files, linting QML components, and validating repository hygiene before commits.

---

## Quick Reference Table

| Workflow / Tool | Scope | Command | Description |
| :--- | :--- | :--- | :--- |
| **Pre-Commit (All)** | Whole Project | `pre-commit run --all-files` | Runs all formatters, linters, and hygiene checks on all files |
| **Pre-Commit (Staged)** | Staged Files | `pre-commit run` | Runs checks only on files staged with `git add` |
| **CMake: Format All** | C++ & QML | `cmake --build build --target format` | Formats all C++ and QML files inplace via CMake |
| **CMake: Format C++** | C++ Files | `cmake --build build --target format-cpp` | Runs `clang-format` on all `src/**/*.cpp` and `src/**/*.h` |
| **CMake: Format QML** | QML Files | `cmake --build build --target format-qml` | Runs `qmlformat` on all `src/ui/**/*.qml` |
| **CMake: Check C++** | C++ Files | `cmake --build build --target format-check-cpp` | Dry-run check with `--Werror` (returns non-zero on diff) |
| **Direct: C++ Format** | C++ Files | `clang-format -i $(find src -name "*.cpp" -o -name "*.h")` | Direct `clang-format` invocation |
| **Direct: QML Format** | QML Files | `qmlformat --inplace $(find src -name "*.qml")` | Direct `qmlformat` invocation |
| **Direct: QML Lint** | QML Files | `qmllint $(find src/ui -name "*.qml")` | Direct `qmllint` invocation |

---

## Recommended Workflow Before Committing

Always ensure code is formatted and passes lint checks prior to staging and committing:

```bash
# 1. Format and lint all tracked files
pre-commit run --all-files

# 2. Stage verified changes
git add <files>

# 3. Commit cleanly
git commit -m "Your commit message"
```

---

## Configuration Files

The project formatting and linting rules are defined in the following root configuration files:

- **`.clang-format`**: C++ code style configuration (LLVM-based, 4-space indentation, 120 column limit).
- **`.pre-commit-config.yaml`**: Pre-commit hook definitions for Git integration.
- **`.qmllint.ini`**: QML linting rules, category disables, and import search paths.
- **`.qmlformat.ini`**: QML formatter styling options and indentation rules.
- **`.editorconfig`**: Cross-editor indentation, newline, and charset consistency.
