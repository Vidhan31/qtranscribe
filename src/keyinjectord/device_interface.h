#pragma once

namespace keyinjectord {

class IDevice {
public:
    virtual ~IDevice() = default;
    virtual bool sendCtrlV() = 0;
};

} // namespace keyinjectord
