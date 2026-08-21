#include "ipc_server.h"

#include "logging.h"

#include <algorithm>
#include <cerrno>
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

namespace {

std::string extractJsonStringValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    auto keyPos = json.find(searchKey);
    if (keyPos == std::string::npos)
        return {};

    auto colonPos = json.find(':', keyPos + searchKey.size());
    if (colonPos == std::string::npos)
        return {};

    auto openQuote = json.find('"', colonPos + 1);
    if (openQuote == std::string::npos)
        return {};

    std::string result;
    for (size_t i = openQuote + 1; i < json.size(); ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            char next = json[i + 1];
            if (next == '"' || next == '\\' || next == '/') {
                result += next;
            } else if (next == 'n') {
                result += '\n';
            } else if (next == 't') {
                result += '\t';
            } else {
                result += next;
            }
            ++i;
        } else if (json[i] == '"') {
            break;
        } else {
            result += json[i];
        }
    }

    return result;
}

} // anonymous namespace

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
    m_clientBuffers.erase(m_clientBuffers.begin() + clientIdx);
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
                            m_clientBuffers.emplace_back();
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
    char buf[kMaxBufferSize];

    ssize_t n = read(fd, buf, sizeof(buf));
    if (n <= 0) {
        disconnectClient(clientIdx);
        return;
    }

    std::string& buffer = m_clientBuffers[clientIdx];
    if (buffer.size() + static_cast<size_t>(n) > kMaxBufferSize) {
        KEYINJECTORD_LOG_ERROR("Client fd=%d exceeded maximum buffer size (%zu bytes), disconnecting", fd,
                               kMaxBufferSize);
        disconnectClient(clientIdx);
        return;
    }

    buffer.append(buf, static_cast<size_t>(n));

    size_t pos;
    while ((pos = buffer.find('\n')) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);

        if (!line.empty()) {
            processMessage(line);

            std::string response = "{\"status\": \"ok\"}\n";
            [[maybe_unused]] auto w = write(fd, response.c_str(), response.size());
        }
    }
}

void IpcServer::processMessage(const std::string& message) {
    KEYINJECTORD_LOG_DEBUG("IPC Received payload: %s", message.c_str());

    std::string cmd = extractJsonStringValue(message, "cmd");
    if (cmd == "paste") {
        m_device.sendCtrlV();
    } else {
        KEYINJECTORD_LOG_ERROR("Unknown IPC command: '%s'", cmd.c_str());
    }
}

} // namespace keyinjectord
