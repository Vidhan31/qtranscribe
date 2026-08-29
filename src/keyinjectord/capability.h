#pragma once

#include <stdexcept>
#include <string>

#include <sys/capability.h>

namespace keyinjectord {

/// Raise CAP_DAC_OVERRIDE from the permitted set into the effective set.
void raiseCapDacOverride();

/// Lower CAP_DAC_OVERRIDE from the effective set (keep in permitted).
void lowerCapDacOverride();

/// Permanently drop CAP_DAC_OVERRIDE from Effective, Permitted, and (if CAP_SETPCAP is available) Bounding sets.
/// Also sets PR_SET_NO_NEW_PRIVS.
void permanentlyDropCapDacOverride();

/// RAII guard: raises CAP_DAC_OVERRIDE on construction, lowers on destruction.
/// Use this around open() calls that need the capability.
class CapabilityGuard {
public:
    CapabilityGuard();
    ~CapabilityGuard();

    CapabilityGuard(const CapabilityGuard&) = delete;
    CapabilityGuard& operator=(const CapabilityGuard&) = delete;

private:
    bool m_raised = false;
};

} // namespace keyinjectord
