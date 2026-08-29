# QTranscribe domain model and glossary

This document defines the core domain concepts and components of QTranscribe.

## Core modules and concepts

### Composition and lifecycle

- **`ApplicationContext`.** Instantiates all core services and wires their dependencies. In GUI mode, it manages QML singletons; in tests, it creates concrete instances directly.

### Dictation and coordination

- **`DictationCoordinator`.** Orchestrates the dictation workflow. It manages state transitions (`Idle`, `Recording`, `Transcribing`, `Enhancing`, `Error`), captures audio via `AudioRecorder`, routes audio to active STT engines, coordinates LLM enhancements, and dispatches text to `TextInjectorClient` or the UI.
- **`AudioFeedbackPlayer`.** Plays audio cues for recording start, stop, and error events based on user settings.
- **`DictationPadModel`.** Manages the text buffer, cursor position, selection ranges, and editing history for the dictation scratchpad.
- **`SystemHealthMonitor`.** Inspects audio input devices, local model files, API keys, shortcut portals, and daemon socket connections to determine overall system readiness.

### Speech-to-text backends

- **`AbstractSttClient`.** Base interface for speech-to-text engines.
- **`WhisperSttClient`.** Runs local speech recognition using `whisper.cpp` and Vulkan compute, offloading inference to `WhisperWorker`.
- **`WhisperWorker`.** Background worker thread managing the `whisper.cpp` context and running inference on PCM audio buffers.
- **`WavDecoder`.** Standalone utility for parsing 16-bit PCM WAV headers and converting audio byte buffers into normalized float arrays for inference.
- **`WhisperModelStorage`.** Manages local model files on disk, directory paths, and active model selection.
- **`WhisperModelCatalog`.** Provides metadata, download URLs, file sizes, and expected SHA-256 checksums for supported Whisper GGML models.
- **`ModelDownloader`.** Handles asynchronous HTTP model downloads and validates SHA-256 checksums against catalog specifications.
- **`GroqSttClient`.** Sends audio recordings to Groq Cloud Whisper API endpoints.

### Cloud and LLM subsystem

- **`GroqApiClient`.** Sends HTTP requests to the Groq Cloud API, attaches authentication headers, and parses rate limit responses.
- **`ApiKeyStore`.** Manages API keys in the system keychain (`libsecret`) with encrypted local file fallback storage.
- **`PresetProvider`.** Supplies system prompts and preset transformation rules for LLM text cleanup and formatting.
- **`GroqLlmClient`.** Cleans up raw transcripts using Groq language models and user-selected presets.

### Desktop and platform integration

- **`GlobalShortcutManager`.** Registers global push-to-talk and toggle shortcuts through `org.freedesktop.portal.GlobalShortcuts`.
- **`TextInjectorClient`.** Sends transcribed text to the `keyinjectord` daemon over a UNIX domain socket for virtual keystroke typing, falling back to clipboard paste if unavailable.
- **`DBusService`.** Exposes `io.github.qtranscribe.SpeechService` on the session D-Bus for remote CLI control.
- **`StatusNotifierService`.** Exports a system tray icon and contextual menu via `StatusNotifierItem` and `DBusMenu`.
- **`TranscriptionModel`.** In-memory list model holding transcription history records.

### Key injection daemon (`keyinjectord`)

- **`IpcServer`.** UNIX domain socket server managing client connections, request deserialization, and command dispatching.
- **`LauncherAuth`.** Authenticates connecting clients by verifying caller UID, procfs metadata, and executable path validity.
- **`FsSecurityChecker`.** Validates binary and directory security by inspecting file ownership, write permissions, and directory ancestry.
- **`ProcfsUtils`.** Reads `/proc` information including executable paths, command lines, mount tables, and file descriptor targets.
- **`SocketCredentials`.** Extracts peer credentials (`ucred` / PID, UID, GID) from UNIX domain socket connections.
- **`KeyboardMacroInjector`.** Synthesizes compound key combinations, modifier keys, and Unicode text typing sequences.
- **`IDevice` / `UinputDevice`.** Abstract device interface and Linux `/dev/uinput` driver for emitting kernel-level input events.

### User interface components (QML)

- **`Main.qml`.** Main application window shell and page navigation host.
- **`SpeechPanel.qml`.** Main dictation view composing recording controls, scratchpad editor, diagnostics column, and status footer.
- **`RecordingControl.qml`.** Dedicated control for recording trigger, engine mode switching, audio visualizer, and live timer.
- **`SystemDiagnosticsColumn.qml`.** Diagnostic card displaying hardware status, engine readiness, and daemon connection health.
- **`SystemStatusFooter.qml`.** Status bar displaying active engine details, registered shortcuts, and audio feedback state.
