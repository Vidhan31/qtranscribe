#pragma once

#include "device_interface.h"
#include "protocol.h"

#include <cstddef>
#include <string>
#include <vector>

#include <poll.h>

namespace keyinjectord {

constexpr size_t kMaxBufferSize = 1024;
constexpr size_t kMaxClients = 8;

class IpcServer {
public:
    explicit IpcServer(const std::string& socketPath, IDevice& device);
    ~IpcServer();

    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    void run();
    void stop();

private:
    void handleClient(int clientIdx);
    void disconnectClient(size_t clientIdx);

    std::string m_socketPath;
    IDevice& m_device;
    int m_listenFd = -1;
    int m_signalFd = -1;
    int m_stopEventFd = -1;

    std::vector<int> m_clientFds;
    std::vector<struct pollfd> m_pollFds;
};

} // namespace keyinjectord
