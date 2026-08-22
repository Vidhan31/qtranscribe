#include "ipc_server.h"

#include "logging.h"

#include "protocol.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

namespace keyinjectord {

IpcServer::IpcServer(int socketFd, IDevice& device)
    : m_socketFd(socketFd)
    , m_device(device) {
    if (m_socketFd < 0) {
        throw std::invalid_argument("Invalid socket file descriptor passed to IpcServer");
    }

    // Create signalfd for SIGINT/SIGTERM — replaces the self-pipe trick.
    // Signals must already be blocked via sigprocmask() before construction.
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    m_signalFd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (m_signalFd < 0) {
        throw std::runtime_error(std::string("signalfd() failed: ") + std::strerror(errno));
    }

    m_stopEventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_stopEventFd < 0) {
        throw std::runtime_error(std::string("eventfd() failed: ") + std::strerror(errno));
    }

    KEYINJECTORD_LOG_INFO("IPC server initialized with inherited socket descriptor (fd=%d)", m_socketFd);
}

IpcServer::~IpcServer() {
    if (m_socketFd >= 0) {
        close(m_socketFd);
        m_socketFd = -1;
    }

    if (m_signalFd >= 0) {
        close(m_signalFd);
        m_signalFd = -1;
    }

    if (m_stopEventFd >= 0) {
        close(m_stopEventFd);
        m_stopEventFd = -1;
    }

    KEYINJECTORD_LOG_INFO("IPC server shut down");
}

void IpcServer::stop() {
    if (m_stopEventFd >= 0) {
        uint64_t val = 1;
        [[maybe_unused]] auto w = write(m_stopEventFd, &val, sizeof(val));
    }
}

void IpcServer::run() {
    KEYINJECTORD_LOG_DEBUG("Event loop started");

    while (true) {
        m_pollFds.clear();
        m_pollFds.push_back({m_signalFd, POLLIN, 0});
        m_pollFds.push_back({m_stopEventFd, POLLIN, 0});
        m_pollFds.push_back({m_socketFd, POLLIN | POLLHUP | POLLERR, 0});

        int ret = poll(m_pollFds.data(), m_pollFds.size(), -1);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            KEYINJECTORD_LOG_ERROR("poll() error: %s", std::strerror(errno));
            break;
        }

        // Check signalfd for shutdown signals
        if (m_pollFds[0].revents & POLLIN) {
            struct signalfd_siginfo sigInfo {};
            [[maybe_unused]] auto n = read(m_signalFd, &sigInfo, sizeof(sigInfo));
            KEYINJECTORD_LOG_INFO("Caught signal %d (%s), shutting down...", sigInfo.ssi_signo,
                                  strsignal(static_cast<int>(sigInfo.ssi_signo)));
            break;
        }

        // Check stop eventfd
        if (m_pollFds[1].revents & POLLIN) {
            uint64_t val = 0;
            [[maybe_unused]] auto n = read(m_stopEventFd, &val, sizeof(val));
            KEYINJECTORD_LOG_INFO("Stop requested, shutting down...");
            break;
        }

        // Check client socket activity or disconnection
        if (m_pollFds[2].revents & (POLLIN | POLLHUP | POLLERR)) {
            if (!handleClientRead()) {
                break;
            }
        }
    }

    KEYINJECTORD_LOG_DEBUG("Event loop exited");
    if (m_socketFd >= 0) {
        close(m_socketFd);
        m_socketFd = -1;
    }
}

bool IpcServer::handleClientRead() {
    uint8_t buf[kMaxBufferSize];

    ssize_t n = read(m_socketFd, buf, sizeof(buf));
    if (n <= 0) {
        if (n == 0) {
            KEYINJECTORD_LOG_INFO("Peer disconnected (EOF), shutting down daemon");
        } else {
            KEYINJECTORD_LOG_ERROR("read() error on socket fd=%d: %s", m_socketFd, std::strerror(errno));
        }
        return false;
    }

    for (ssize_t i = 0; i < n; ++i) {
        uint8_t opcodeRaw = buf[i];
        ResponseStatus status;
        bool shouldDisconnect = false;

        switch (static_cast<Opcode>(opcodeRaw)) {
            case Opcode::Paste:
                KEYINJECTORD_LOG_DEBUG("IPC Received command: Paste (0x%02X)", opcodeRaw);
                if (m_device.sendCtrlV()) {
                    status = ResponseStatus::Ok;
                } else {
                    KEYINJECTORD_LOG_ERROR("Device sendCtrlV() failed");
                    status = ResponseStatus::DeviceError;
                }
                break;
            case Opcode::Ping:
                KEYINJECTORD_LOG_DEBUG("IPC Received command: Ping (0x%02X)", opcodeRaw);
                status = ResponseStatus::Ok;
                break;
            default:
                KEYINJECTORD_LOG_ERROR("Unknown IPC opcode: 0x%02X from fd=%d", opcodeRaw, m_socketFd);
                status = ResponseStatus::UnknownCmd;
                shouldDisconnect = true;
                break;
        }

        uint8_t respByte = static_cast<uint8_t>(status);
        [[maybe_unused]] auto w = write(m_socketFd, &respByte, sizeof(respByte));

        if (shouldDisconnect) {
            return false;
        }
    }

    return true;
}

} // namespace keyinjectord
