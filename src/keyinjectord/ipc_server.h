#pragma once

#include "device_interface.h"
#include "protocol.h"

#include <cstddef>
#include <vector>

#include <poll.h>

namespace keyinjectord {

constexpr size_t kMaxBufferSize = 1024;

class IpcServer {
public:
    explicit IpcServer(int socketFd, IDevice& device);
    ~IpcServer();

    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    void run();
    void stop();

private:
    bool handleClientRead();

    int m_socketFd = -1;
    IDevice& m_device;
    int m_signalFd = -1;
    int m_stopEventFd = -1;
    std::vector<struct pollfd> m_pollFds;
};

} // namespace keyinjectord
