#pragma once

#include "uinput_device.h"

#include <string>
#include <vector>

#include <poll.h>

namespace keyinjectord {

class IpcServer {
public:
    explicit IpcServer(const std::string& socketPath, UinputDevice& device);
    ~IpcServer();

    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    void run();

private:
    void handleClient(int clientFd);
    void processMessage(const std::string& message);

    std::string m_socketPath;
    UinputDevice& m_device;
    int m_listenFd = -1;
    int m_signalFd = -1;

    std::vector<int> m_clientFds;
    std::vector<std::string> m_clientBuffers;
    std::vector<struct pollfd> m_pollFds;
};

} // namespace keyinjectord
