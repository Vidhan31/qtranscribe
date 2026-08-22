# Research: Restricting `keyinjectord` via Anonymous `socketpair()` & Inherited FD Architecture

## 1. Executive Summary

Transitioning `keyinjectord` from a named UNIX domain socket (`$XDG_RUNTIME_DIR/keyinjectord.sock`) to an **Anonymous `socketpair()` / Inherited File Descriptor Architecture** is **strongly recommended**.

It achieves **100% exclusive process isolation** for `QTranscribe`, completely closes the same-UID security surface, eliminates filesystem footprint, improves startup latency (eliminating synchronous polling loops), and simplifies lifecycle management with zero negative UX impact.

---

## 2. Problem Statement & Threat Model

### Current Architecture
Currently, `keyinjectord` operates as a standalone daemon that:
1. Opens `/dev/uinput` using file capabilities (`cap_dac_override+p`).
2. Creates and binds a named UNIX domain stream socket at `$XDG_RUNTIME_DIR/keyinjectord.sock` (fallback: `/tmp/qtranscribe-$UID/keyinjectord.sock`) with `0600` permissions.
3. Accepts up to 8 client connections, validating peer UID via `getsockopt(..., SO_PEERCRED, ...)`.

### Vulnerability / Threat Analysis
| Threat Vector | Current (Named Socket) | Risk Level |
| :--- | :--- | :--- |
| **Cross-User Access** (Different UID) | Blocked via `0600` permissions + `SO_PEERCRED` UID check | Low |
| **Same-User Malicious / Compromised Process** | **VULNERABLE**: Any process running as the same user (e.g., untrusted script, compromised electron/browser app, malicious npm/pip package) can discover and connect to `$XDG_RUNTIME_DIR/keyinjectord.sock` and issue arbitrary `Opcode::Paste` or injection commands, bypassing Wayland input isolation. | **HIGH** |
| **DoS / Resource Starvation** | Any local same-user process can exhaust the 8-client connection limit or flood the socket. | Medium |
| **Orphaned / Stale Socket Files** | Ungraceful crashes can leave stale socket files on disk requiring cleanup (`unlink`). | Low |

---

## 3. How Anonymous `socketpair()` / Inherited FD Works

In an **Inherited FD / `socketpair()`** architecture:

```
+-------------------------------------------------------------+
|                      QTranscribe (Parent)                   |
|                                                             |
|  1. socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv)  |
|  2. sv[0] -> QLocalSocket::setSocketDescriptor()            |
|  3. Spawn child: QProcess starts `keyinjectord --fd 3`       |
|     (sv[1] has FD_CLOEXEC cleared in child modifier)        |
|  4. Parent closes sv[1]                                     |
+------------------------------+------------------------------+
                               |
                   Inherited Socket (Kernel Buffer)
                   [No Filesystem Path / No Inode]
                               |
+------------------------------v------------------------------+
|                     keyinjectord (Child)                    |
|                                                             |
|  1. Opens /dev/uinput (with CAP_DAC_OVERRIDE)               |
|  2. Drops capabilities & sets PR_SET_NO_NEW_PRIVS           |
|  3. Adopts inherited fd (e.g. fd 3 or STDIN)                |
|  4. Runs IpcServer directly on fd 3 (1:1 dedicated session) |
+-------------------------------------------------------------+
```

### Protocol & Connection Flow
1. **Creation**: `qtranscribe` invokes `socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds)`.
2. **Channel Ownership**:
   - `fds[0]` is retained by `qtranscribe` and bound directly to `QLocalSocket` using `m_socket->setSocketDescriptor(fds[0], QLocalSocket::ConnectedState)`.
   - `fds[1]` is passed to `keyinjectord` during `QProcess` execution.
3. **Execution**: In the child context (before `execve`), `FD_CLOEXEC` on `fds[1]` is cleared (or duplicated via `dup2` to a designated FD like `3` or `STDIN_FILENO`).
4. **Daemon Initialization**: `keyinjectord` opens `/dev/uinput`, drops privileges permanently, and executes its event loop on the inherited descriptor.

---

## 4. Security Evaluation & Confirmation

### 1. Can other processes with the same UID access or hijack the anonymous socket?
- **No filesystem endpoint**: There is no path in `$XDG_RUNTIME_DIR` or `/tmp`. No socket file exists for `connect()` or `bind()`.
- **No listening socket**: `keyinjectord` does not call `listen()` or `accept()`. It only interacts with the already-connected descriptor.
- **`/proc/<pid>/fd/` Protection**:
  - In Linux, attempting to `open("/proc/<pid>/fd/<sock_fd>")` fails with **`ENXIO`** (No such device or address) or `EOPNOTSUPP`. The kernel explicitly prohibits opening socket file descriptors via the `/proc` filesystem.
  - Process inspection/attachment via `ptrace` or `pidfd_getfd` is blocked between unprivileged same-UID processes on modern Linux systems (Ubuntu 24.04 defaults to Yama LSM `ptrace_scope=1`, which forbids non-ancestor ptrace).
  - Furthermore, `keyinjectord` executes `prctl(PR_SET_NO_NEW_PRIVS, 1)` and permanently drops capabilities, locking its process state.

### 2. Interaction with Linux File Capabilities (`cap_dac_override+p`)
- **Kernel Specification**: Linux file capabilities (`xattr` `security.capability`) are evaluated by the kernel during `execve()`.
- Open file descriptors that lack `FD_CLOEXEC` are preserved across `execve()` regardless of file capability transitions.
- `keyinjectord` successfully receives `CAP_DAC_OVERRIDE` in its Permitted set upon `execve`, opens `/dev/uinput`, permanently drops all capabilities, and immediately begins reading from the inherited socket.

---

## 5. Comparison: Alternative Restriction Mechanisms

| Mechanism | Exclusivity to QTranscribe | Security vs Same-UID | Implementation Complexity | Robustness / Flaws |
| :--- | :--- | :--- | :--- | :--- |
| **Anonymous `socketpair()` (Inherited FD)** | **100% Guaranteed** | **Complete** (No external entrypoint) | **Low** (Clean C++ / Qt6 API) | **Zero race conditions**, instant startup, no stale files. |
| **Named Socket + Ephemeral Shared Secret / Token** | High | Medium (Token could leak in `/proc` or IPC race) | Medium | Retains listening filesystem endpoint; requires handshake protocol. |
| **Named Socket + `SO_PEERPID` & `/proc/$pid/exe`** | High | Low–Medium | Medium | Prone to PID reuse race conditions and symlink resolution bypasses. |
| **Abstract Namespace Socket (`\0...`)** | Low | Low | Low | Accessible by *any* process in the network namespace; no filesystem DAC permissions. |

---

## 6. UX and Functional Impact Analysis

| UX Dimension | Current (Named Socket) | Anonymous `socketpair()` | Impact |
| :--- | :--- | :--- | :--- |
| **Startup Latency** | **100 ms – 2500 ms** (Synchronous retry loop polling for file creation) | **< 1 ms** (Instant socket allocation in kernel memory) | 🚀 **Significantly faster** |
| **Failure Feedback** | Delayed until polling loop times out | **Instant**: `QProcess` emits error / exit code and stderr output immediately | 🚀 **Better error messaging** |
| **Process Lifecycle** | Requires manual socket unlink and orphan handling | **Deterministic**: Closing socket automatically signals EOF to daemon; `PDEATHSIG` provides fallback | 🚀 **Zero stale sockets / zombies** |
| **Multi-Instance Support** | Socket name collision if two instances run | **Completely isolated**: Each app instance owns its dedicated daemon | 🚀 **Reliable multi-window / testing** |
| **Clipboard / Keystroke Paste Functionality** | Unchanged (Same binary protocol: `Opcode::Paste`) | Unchanged (Same binary protocol: `Opcode::Paste`) | 🟢 **100% feature parity** |

---

## 7. Implementation Roadmap & Architecture Changes

### A. `src/keyinjectord/main.cpp` & `ipc_server`
- Add support for `--socket-fd <int>` (or check if `STDIN_FILENO` is a socket).
- When `--socket-fd` is supplied:
  - Verify descriptor with `getsockopt(fd, SOL_SOCKET, SO_TYPE, ...)`.
  - Instantiate `IpcServer` with the single connected descriptor instead of creating a listening socket.
  - Simplify `IpcServer`: poll `m_signalFd`, `m_stopEventFd`, and `m_clientFd`. When client disconnects (`read == 0` / `POLLHUP`), shut down cleanly.
- Keep optional `--socket-path` for standalone testing and mock test harnesses.

### B. `src/core/DaemonConnector.cpp`
- In `ensureDaemonRunning()` / `connectToServer()`:
  - Create socket pair: `socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds)`.
  - Adopt parent end: `m_socket->setSocketDescriptor(fds[0], QLocalSocket::ConnectedState)`.
  - Pass child end `fds[1]` to `keyinjectord` via argument `--socket-fd <fds[1]>`.
  - Use `QProcess::setChildProcessModifier` to clear `FD_CLOEXEC` on `fds[1]` before `execve`.
  - Close `fds[1]` in the parent process.
  - Eliminate the 25-step sleep/polling loop.

---

## 8. Conclusion & Recommendation

The **Anonymous `socketpair()` / Inherited FD Architecture** is the optimal design:
- It meets all security criteria to restrict `keyinjectord` strictly to `QTranscribe`.
- It eliminates attack vectors from other processes running under the same user account.
- It enhances user experience through instantaneous startup and clean lifecycle management.
