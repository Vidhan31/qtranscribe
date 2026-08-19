## Code Quality & Architecture
- Keep code modular, secure, and efficient.
- Maintain strict separation of concerns.
- Strive for the best possible solutions before settling for hacky workarounds.
- If a solution requires overly long explanations to justify, it is probably not good enough.
- Prefer QML over QT Widgets.
- Prefer declarative style over imperative.
- No premature optimizations.
- Keep styling configuration centralized.

## Execution & Safety
- Do not run destructive commands (e.g., `rm -rf`, `dnf remove`) unless explicitly instructed.
- Do not install anything unless explicitly asked.
- Do not run app executables unless explicitly given permission.
- **Do not run Docker scripts directly.** Ask the user to run them.

## Platform & Environment
- Application is strictly Wayland only. No fallback to X11.
- Ubuntu 24.04, GNOME 46 is considered as baseline.
