#include "ipc_server.h"

#include "logging.h"

#include "protocol.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

namespace keyinjectord {

IpcServer::IpcServer(const std::string& socketPath, IDevice& device)
    : m_socketPath(socketPath)
    , m_device(device) {
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

    unlink(m_socketPath.c_str());

    m_listenFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (m_listenFd < 0) {
        throw std::runtime_error(std::string("socket() failed: ") + std::strerror(errno));
    }

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    if (m_socketPath.size() >= sizeof(addr.sun_path)) {
        throw std::runtime_error("Socket path too long: " + m_socketPath);
    }
    std::strncpy(addr.sun_path, m_socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(m_listenFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        throw std::runtime_error(std::string("bind() failed on ") + m_socketPath + ": " + std::strerror(errno));
    }

    // Restrict socket access to owner only (defense-in-depth on top of XDG_RUNTIME_DIR permissions)
    if (fchmod(m_listenFd, 0600) != 0) {
        KEYINJECTORD_LOG_ERROR("fchmod(0600) on socket fd failed: %s", std::strerror(errno));
    }

    if (listen(m_listenFd, 5) != 0) {
        throw std::runtime_error(std::string("listen() failed: ") + std::strerror(errno));
    }

    KEYINJECTORD_LOG_INFO("IPC server listening on %s", m_socketPath.c_str());
}

IpcServer::~IpcServer() {
    for (int fd : m_clientFds) {
        close(fd);
    }

    if (m_listenFd >= 0) {
        close(m_listenFd);
    }

    unlink(m_socketPath.c_str());

    if (m_signalFd >= 0) {
        close(m_signalFd);
    }

    if (m_stopEventFd >= 0) {
        close(m_stopEventFd);
    }

    KEYINJECTORD_LOG_INFO("IPC server shut down, socket removed");
}

void IpcServer::stop() {
    if (m_stopEventFd >= 0) {
        uint64_t val = 1;
        [[maybe_unused]] auto w = write(m_stopEventFd, &val, sizeof(val));
    }
}

void IpcServer::disconnectClient(size_t clientIdx) {
    int fd = m_clientFds[clientIdx];
    KEYINJECTORD_LOG_DEBUG("Client disconnected (fd=%d)", fd);
    close(fd);
    m_clientFds.erase(m_clientFds.begin() + clientIdx);
}

void IpcServer::run() {
    KEYINJECTORD_LOG_DEBUG("Event loop started");

    while (true) {
        m_pollFds.clear();
        m_pollFds.reserve(3 + m_clientFds.size());

        m_pollFds.push_back({m_signalFd, POLLIN, 0});
        m_pollFds.push_back({m_stopEventFd, POLLIN, 0});
        m_pollFds.push_back({m_listenFd, POLLIN, 0});
        for (int cfd : m_clientFds) {
            m_pollFds.push_back({cfd, POLLIN, 0});
        }

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

        // Accept new client connections with peer credential verification
        if (m_pollFds[2].revents & POLLIN) {
            int clientFd = accept4(m_listenFd, nullptr, nullptr, SOCK_CLOEXEC);
            if (clientFd >= 0) {
                if (m_clientFds.size() >= kMaxClients) {
                    KEYINJECTORD_LOG_WARN("Max client connections reached (%zu/%zu), rejecting connection fd=%d",
                                          m_clientFds.size(), kMaxClients, clientFd);
                    close(clientFd);
                } else {
                    // Verify peer credentials — reject connections from other users
                    struct ucred peerCreds {};
                    socklen_t credLen = sizeof(peerCreds);
                    if (getsockopt(clientFd, SOL_SOCKET, SO_PEERCRED, &peerCreds, &credLen) == 0) {
                        if (peerCreds.uid != getuid()) {
                            KEYINJECTORD_LOG_ERROR("Rejected connection from UID %d (expected %d), PID %d",
                                                   peerCreds.uid, getuid(), peerCreds.pid);
                            close(clientFd);
                        } else {
                            KEYINJECTORD_LOG_DEBUG("Client connected (fd=%d, pid=%d)", clientFd, peerCreds.pid);
                            m_clientFds.push_back(clientFd);
                        }
                    } else {
                        KEYINJECTORD_LOG_ERROR("getsockopt(SO_PEERCRED) failed: %s — rejecting connection",
                                               std::strerror(errno));
                        close(clientFd);
                    }
                }
            }
        }

        for (size_t i = m_pollFds.size() - 1; i >= 3; --i) {
            size_t clientIdx = i - 3;
            if (m_pollFds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                handleClient(static_cast<int>(clientIdx));
            }
        }
    }

    KEYINJECTORD_LOG_DEBUG("Event loop exited");
}

void IpcServer::handleClient(int clientIdx) {
    int fd = m_clientFds[clientIdx];
    uint8_t buf[kMaxBufferSize];

    ssize_t n = read(fd, buf, sizeof(buf));
    if (n <= 0) {
        disconnectClient(clientIdx);
        return;
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
                KEYINJECTORD_LOG_ERROR("Unknown IPC opcode: 0x%02X from fd=%d", opcodeRaw, fd);
                status = ResponseStatus::UnknownCmd;
                shouldDisconnect = true;
                break;
        }

        uint8_t respByte = static_cast<uint8_t>(status);
        [[maybe_unused]] auto w = write(fd, &respByte, sizeof(respByte));

        if (shouldDisconnect) {
            disconnectClient(clientIdx);
            return;
        }
    }
}

} // namespace keyinjectord
