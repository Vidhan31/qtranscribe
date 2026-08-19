# QTranscribe

A speech-to-text application for Linux on Wayland. Press a shortcut, speak into your microphone, press it again, and your words are typed directly into the active text field.

Transcription is powered by Groq's Whisper API. Requires a free [Groq API key](https://console.groq.com/keys). Native Qt 6 / QML, strictly Wayland, zero Electron.

---

## Features

- **Types anywhere.** Inserts text directly into whichever app you are using, from browsers and document editors to chat apps and terminals, without messing with your clipboard.
- **Fast, accurate dictation.** Transcribes speech near-instantly with proper punctuation and capitalization.
- **Multi-language support.** Automatically recognizes what language you are speaking, or lets you choose a specific one.
- **Custom vocabulary.** Add specialized terms, acronyms, or names so words are not misspelled.
- **Global shortcut and tray.** Start and stop recording with a custom keyboard shortcut from anywhere, or use the system tray icon.
- **Audio feedback.** Plays subtle audio cues when recording starts and stops so you do not have to watch the screen.
- **Secure key storage.** Safely stores your API key in your system password manager, never in plaintext configuration files.
- **Transcription history.** Search, review, and copy your past dictations anytime.

---

## Installation

Pre-built binaries are available under [GitHub Releases](../../releases).

### Debian and Ubuntu (`.deb`)

```bash
sudo apt install ./qtranscribe_*_amd64.deb
```

Installs to `/opt/qtranscribe`, adds `/usr/bin/qtranscribe`, configures desktop integration, and grants required capabilities to `keyinjectord`.

To uninstall:
```bash
sudo apt remove qtranscribe
```

### Fedora and RHEL (`.rpm`)

```bash
sudo dnf install ./qtranscribe-*.x86_64.rpm
```

Installs to `/usr/bin`, registers desktop files, and sets required permissions.

To uninstall:
```bash
sudo dnf remove qtranscribe
```

### Arch Linux (`.pkg.tar.zst` / AUR)

```bash
# Pre-built package
sudo pacman -U ./qtranscribe-*-x86_64.pkg.tar.zst

# AUR
yay -S qtranscribe
```

To uninstall:
```bash
sudo pacman -R qtranscribe
```

### Portable tarball (`.tar.gz`)

```bash
# 1. Extract archive
tar -xzf QTranscribe-*-Linux-x86_64.tar.gz
cd QTranscribe-*-Linux-x86_64

# 2. Grant uinput permission to helper daemon
sudo setcap "cap_dac_override+p" bin/keyinjectord

# 3. Launch
./bin/qtranscribe
```

---

## Desktop environment support and Wayland notes

Tested on modern Wayland compositors:

| Desktop environment | Status | Global shortcuts portal | Notes |
| :--- | :---: | :---: | :--- |
| **KDE Plasma 6** | Supported | Yes (`org.freedesktop.portal.GlobalShortcuts`) | Native KWallet integration and system Qt theming |
| **GNOME 50+** | Supported | Yes (`org.freedesktop.portal.GlobalShortcuts`) | Uses GNOME Keyring / Secret Service |
| **GNOME 46** | Supported | No (manual setup) | Map a custom shortcut to `qtranscribe --toggle` in Settings |
| **COSMIC** (System76) | Supported | No (manual setup) | Map a custom shortcut to `qtranscribe --toggle` in Settings |

### Architecture and security

- **Wayland only.** Built specifically for Wayland compositors. X11 is not supported.
- **Global shortcuts:**
  - On **KDE Plasma 6** and **GNOME 50+**, the desktop portal handles global shortcuts automatically and prompts on first launch.
  - On compositors without the GlobalShortcuts portal (such as **GNOME 46** or **COSMIC**), configure a custom shortcut in system settings:
    1. Open **Settings** -> **Keyboard** -> **Keyboard Shortcuts**.
    2. Add a custom shortcut (for example `Ctrl+Shift+Space` or `Super+Space`).
    3. Set the command to: `qtranscribe --toggle`
- **Key injection permissions (`keyinjectord`).** Wayland prevents unprivileged applications from injecting input events into other windows. To type at the cursor, `keyinjectord` writes to `/dev/uinput`. It requests `cap_dac_override` only to open `/dev/uinput` during initialization, immediately drops all capabilities permanently, and locks the process using `PR_SET_NO_NEW_PRIVS` (see [`src/keyinjectord/capability.cpp`](src/keyinjectord/capability.cpp)).
- **Backend service.** Audio is sent to Groq's cloud API for transcription. For fully offline dictation without network requests, consider tools built on local `whisper.cpp`.

---

## Usage

### 1. Configure the API key
1. Launch QTranscribe from your application menu or run `qtranscribe`.
2. Open **Settings** from the tray icon or main window, enter your [Groq API key](https://console.groq.com/keys), and save.

### 2. Configure the hotkey
- **Plasma 6 / GNOME 50+:** Accept the shortcut prompt on first launch.
- **COSMIC / GNOME 46:** Add a custom keyboard shortcut mapped to `qtranscribe --toggle`.

### 3. Dictate
1. Click into any text field (browser, terminal, editor, or chat app).
2. Press your shortcut (or click the tray icon, or run `qtranscribe --toggle`).
3. Speak, press the shortcut again to finish recording, and the transcribed text types directly into the focused field.

---

## Tips

- **Mouse binding.** Map `qtranscribe --toggle` to an extra mouse button using your mouse configuration tool (such as Piper or input-remapper) for one-click dictation.
- **Autostart.** Enable start on login in Settings so the tray icon and shortcut listener are active upon boot.

---

## Building from source

### Prerequisites

- CMake >= 3.22
- Ninja
- C++20 compiler (GCC 13+ or Clang 17+)
- Qt 6 (Core, Gui, Quick, Qml, Multimedia, WaylandClient)
- QtKeychain (Qt6)
- `libevdev` and `libcap` development headers

### 1. Build `keyinjectord`
Build the daemon once and grant required capabilities. Because it builds in its own directory (`build-keyinjectord/`), rebuilding the GUI will not overwrite or clear the capability bit:

```bash
cmake -S src/keyinjectord -B build-keyinjectord -G Ninja && cmake --build build-keyinjectord
sudo setcap "cap_dac_override+p" build-keyinjectord/keyinjectord
```

### 2. Build GUI application
The `linux-qt6-debug` preset builds the GUI app and connects to `build-keyinjectord/keyinjectord`:

```bash
cmake --preset linux-qt6-debug && cmake --build build
```

### 3. Full release build
To build both targets together:

```bash
cmake --preset linux-qt6-release && cmake --build --preset build-release
sudo setcap "cap_dac_override+p" build-release/src/keyinjectord/keyinjectord
```

---

## Packaging

Scripts in [`packaging/`](packaging/) build packages inside Docker containers:

<details>
<summary><strong>Debian and Ubuntu (.deb)</strong></summary>

```bash
# Build package (outputs to dist/deb/):
./packaging/deb/build-deb.sh 24.04 1.0.0

# Or with Docker Buildx:
docker buildx build --file packaging/deb/Dockerfile.deb --target export -o dist/deb .

# Install:
sudo apt install ./dist/deb/qtranscribe_1.0.0_amd64.deb
```
</details>

<details>
<summary><strong>Fedora and RHEL RPM (.rpm)</strong></summary>

```bash
# Build package (outputs to dist/rpm/):
./packaging/rpm/build-rpm.sh 44 1.0.0 1

# Or with Docker Buildx:
docker buildx build --file packaging/rpm/Dockerfile.rpm --target export -o dist/rpm .

# Install:
sudo dnf install ./dist/rpm/qtranscribe-1.0.0-1.fc44.x86_64.rpm
```
</details>

<details>
<summary><strong>Arch Linux (.pkg.tar.zst)</strong></summary>

```bash
# Build package (outputs to dist/arch/):
./packaging/arch/build-arch.sh 1.0.0 1

# Or with makepkg on Arch:
cd packaging/arch && makepkg -sfc

# Install:
sudo pacman -U ./dist/arch/qtranscribe-1.0.0-1-x86_64.pkg.tar.zst
```
</details>

<details>
<summary><strong>Portable tarball (.tar.gz)</strong></summary>

```bash
# Build tarball (outputs to dist/tarball/):
./packaging/tarball/build-tarball.sh 1.0.0

# Or locally with CPack:
cmake --preset linux-qt6-release && cmake --build --preset build-release --target package
```
</details>

<details>
<summary><strong>CI / release builds</strong></summary>

Pre-built packages (`.deb`, `.rpm`, `.pkg.tar.zst`, `.tar.gz`) and SHA-256 checksums are generated automatically on release tags.
</details>

---

## Troubleshooting

- **Nothing types into the target field:**
  - Verify the target input field is active and focused.
  - If running from source or a portable tarball, verify capability permissions: `sudo setcap "cap_dac_override+p" path/to/keyinjectord`. Distro packages configure this automatically.
  - Run `qtranscribe` in a terminal to inspect diagnostic logs.
- **Shortcuts do not trigger on COSMIC or GNOME 46:**
  - These compositors do not implement the XDG Global Shortcuts portal. Configure a custom shortcut in system settings mapped to `qtranscribe --toggle`.
- **Transcription errors or timeouts:**
  - Verify your Groq API key in Settings and check internet connectivity. Launch `qtranscribe` from a terminal to see HTTP response details.
  - Note that Groq free-tier rate limits may apply during heavy usage.
- **API key fails to save or load:**
  - Ensure a Secret Service provider or keyring daemon (KWallet, GNOME Keyring) is active and unlocked in your session.
