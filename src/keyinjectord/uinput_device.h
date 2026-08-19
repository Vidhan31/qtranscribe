#pragma once

#include <chrono>

#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

namespace keyinjectord {

class UinputDevice {
public:
    explicit UinputDevice(int uinputFd, int delayMs = 18);
    ~UinputDevice();

    UinputDevice(const UinputDevice&) = delete;
    UinputDevice& operator=(const UinputDevice&) = delete;

    bool sendCtrlV();

    bool isValid() const { return m_uidev != nullptr; }

private:
    void emitEvent(int type, int code, int value);

    struct libevdev* m_dev = nullptr;
    struct libevdev_uinput* m_uidev = nullptr;
    int m_fd = -1;
    std::chrono::milliseconds m_delay;
};

} // namespace keyinjectord
