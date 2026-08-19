#include "capability.h"
#include "ipc_server.h"
#include "logging.h"
#include "uinput_device.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <sys/prctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace {

std::string getSocketPath() {
    const char* xdgRuntime = std::getenv("XDG_RUNTIME_DIR");
    if (xdgRuntime && xdgRuntime[0] != '\0') {
        return std::string(xdgRuntime) + "/keyinjectord.sock";
    }

    // Secure fallback: user-isolated directory in /tmp with 0700 permissions
    std::string userDir = "/tmp/qtranscribe-" + std::to_string(getuid());
    mkdir(userDir.c_str(), 0700);
    return userDir + "/keyinjectord.sock";
}

void printUsage(const char* progName) {
    std::fprintf(stderr,
                 "Usage: %s [OPTIONS]\n"
                 "\n"
                 "Options:\n"
                 "  --delay-ms <N>     Inter-key delay in milliseconds (default: 18)\n"
                 "  --socket-path <P>  Unix socket path (default: $XDG_RUNTIME_DIR/keyinjectord.sock)\n"
                 "  -v, --verbose      Enable verbose output logging\n"
                 "  --help             Show this help\n"
                 "\n"
                 "Privilege setup:\n"
                 "  sudo setcap \"cap_dac_override+p\" %s\n"
                 "\n",
                 progName, progName);
}

} // namespace

int main(int argc, char* argv[]) {
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    // Block SIGINT/SIGTERM early — IpcServer will consume them via signalfd.
    // Must happen before any threads are created so child threads inherit the mask.
    sigset_t blockMask;
    sigemptyset(&blockMask);
    sigaddset(&blockMask, SIGINT);
    sigaddset(&blockMask, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &blockMask, nullptr) != 0) {
        KEYINJECTORD_LOG_ERROR("sigprocmask(SIG_BLOCK) failed: %s", std::strerror(errno));
        return 1;
    }

    int delayMs = 18;
    [[maybe_unused]] bool verbose = false;

    std::string socketPath = getSocketPath();

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--delay-ms") == 0 && i + 1 < argc) {
            delayMs = std::atoi(argv[++i]);
            if (delayMs < 1)
                delayMs = 1;
        } else if (std::strcmp(argv[i], "--socket-path") == 0 && i + 1 < argc) {
            socketPath = argv[++i];
        } else if (std::strcmp(argv[i], "--verbose") == 0 || std::strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            KEYINJECTORD_LOG_ERROR("Unknown option: %s", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }

    KEYINJECTORD_LOG_DEBUG("Starting with delay=%dms, socket=%s", delayMs, socketPath.c_str());

    // Open /dev/uinput with CAP_DAC_OVERRIDE raised temporarily
    int uinputFd;
    {
        KEYINJECTORD_LOG_DEBUG("Opening /dev/uinput with capability...");
        keyinjectord::CapabilityGuard capGuard;

        uinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
        if (uinputFd < 0) {
            KEYINJECTORD_LOG_ERROR("Failed to open /dev/uinput: %s", std::strerror(errno));
            KEYINJECTORD_LOG_ERROR("Did you run: sudo setcap \"cap_dac_override+p\" %s ?", argv[0]);
            return 1;
        }
        // CapabilityGuard destructor automatically lowers CAP_DAC_OVERRIDE from Effective set
    }

    KEYINJECTORD_LOG_DEBUG("/dev/uinput opened successfully (fd=%d)", uinputFd);

    // Permanently drop privileges
    try {
        keyinjectord::permanentlyDropCapDacOverride();
    } catch (const std::exception& e) {
        KEYINJECTORD_LOG_ERROR("FATAL: Could not drop capabilities: %s", e.what());
        return 1;
    }

    try {
        keyinjectord::UinputDevice device(uinputFd, delayMs);

        keyinjectord::IpcServer server(socketPath, device);

        server.run();

        KEYINJECTORD_LOG_DEBUG("Clean shutdown complete");
        return 0;

    } catch (const std::exception& e) {
        KEYINJECTORD_LOG_ERROR("FATAL: %s", e.what());
        return 1;
    }
}
