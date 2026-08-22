#include "launcher_auth.h"

#include "logging.h"

#include <cerrno>
#include <climits>
#include <cstring>
#include <filesystem>
#include <string>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>

namespace keyinjectord {

const char* authResultToString(AuthResult result) {
    switch (result) {
        case AuthResult::Success:
            return "Success";
        case AuthResult::InvalidFd:
            return "Invalid file descriptor";
        case AuthResult::NotASocket:
            return "File descriptor is not a SOCK_STREAM socket";
        case AuthResult::NotConnectedUnixSocket:
            return "File descriptor is not a connected AF_UNIX socket";
        case AuthResult::PeerCredsFailed:
            return "Failed to retrieve SO_PEERCRED from socket";
        case AuthResult::UidMismatch:
            return "Peer UID does not match current process UID";
        case AuthResult::PidMismatch:
            return "Peer PID does not match parent process PID";
        case AuthResult::ParentExeReadFailed:
            return "Failed to resolve parent executable path from /proc";
        case AuthResult::SelfExeReadFailed:
            return "Failed to resolve self executable path from /proc";
        case AuthResult::UnauthorizedExecutable:
            return "Parent executable name is not authorized";
        case AuthResult::StatFailed:
            return "Failed to stat parent executable binary";
        case AuthResult::NotRegularFile:
            return "Parent executable is not a regular file";
        case AuthResult::WorldWritable:
            return "Parent executable file is world-writable";
        case AuthResult::UntrustedLocation:
            return "Parent executable is located in an untrusted directory";
    }
    return "Unknown error";
}

bool authorizeLauncher(int socketFd, AuthResult* outResult) {
    auto setResult = [outResult](AuthResult res) {
        if (outResult) {
            *outResult = res;
        }
        return res == AuthResult::Success;
    };

    if (socketFd < 0 || fcntl(socketFd, F_GETFD) == -1) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: invalid socket descriptor %d (%s)", socketFd,
                               std::strerror(errno));
        return setResult(AuthResult::InvalidFd);
    }

    int type = 0;
    socklen_t typeLen = sizeof(type);
    if (getsockopt(socketFd, SOL_SOCKET, SO_TYPE, &type, &typeLen) != 0 || type != SOCK_STREAM) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: descriptor %d is not a SOCK_STREAM socket", socketFd);
        return setResult(AuthResult::NotASocket);
    }

    struct sockaddr_storage addr {};
    socklen_t addrLen = sizeof(addr);
    if (getpeername(socketFd, reinterpret_cast<struct sockaddr*>(&addr), &addrLen) != 0 || addr.ss_family != AF_UNIX) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: descriptor %d is not an AF_UNIX connected socket",
                               socketFd);
        return setResult(AuthResult::NotConnectedUnixSocket);
    }

    struct ucred peerCreds {};
    socklen_t credLen = sizeof(peerCreds);
    if (getsockopt(socketFd, SOL_SOCKET, SO_PEERCRED, &peerCreds, &credLen) != 0) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: getsockopt(SO_PEERCRED) error: %s",
                               std::strerror(errno));
        return setResult(AuthResult::PeerCredsFailed);
    }

    if (peerCreds.uid != getuid()) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: peer UID %d does not match process UID %d",
                               peerCreds.uid, getuid());
        return setResult(AuthResult::UidMismatch);
    }

    pid_t parentPid = getppid();
    if (peerCreds.pid != parentPid && peerCreds.pid != getpid()) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: peer PID %d does not match parent PID %d", peerCreds.pid,
                               parentPid);
        return setResult(AuthResult::PidMismatch);
    }

    if (peerCreds.pid <= 1) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: invalid peer PID %d", peerCreds.pid);
        return setResult(AuthResult::PidMismatch);
    }

    char parentExeBuf[PATH_MAX];
    std::string procParentExe = "/proc/" + std::to_string(peerCreds.pid) + "/exe";
    ssize_t parentLen = readlink(procParentExe.c_str(), parentExeBuf, sizeof(parentExeBuf) - 1);
    if (parentLen <= 0) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: readlink(%s) error: %s", procParentExe.c_str(),
                               std::strerror(errno));
        return setResult(AuthResult::ParentExeReadFailed);
    }
    parentExeBuf[parentLen] = '\0';
    std::filesystem::path parentExePath(parentExeBuf);

    char selfExeBuf[PATH_MAX];
    ssize_t selfLen = readlink("/proc/self/exe", selfExeBuf, sizeof(selfExeBuf) - 1);
    if (selfLen <= 0) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: readlink(/proc/self/exe) error: %s",
                               std::strerror(errno));
        return setResult(AuthResult::SelfExeReadFailed);
    }
    selfExeBuf[selfLen] = '\0';
    std::filesystem::path selfExePath(selfExeBuf);

    std::string parentExeName = parentExePath.filename().string();
    if (parentExeName != "qtranscribe" && parentExeName != "test_ipc_server" &&
        parentExeName != "test_transcription_pipeline") {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: unauthorized parent executable name '%s' at '%s'",
                               parentExeName.c_str(), parentExePath.c_str());
        return setResult(AuthResult::UnauthorizedExecutable);
    }

    struct stat parentStat {};
    if (stat(parentExePath.c_str(), &parentStat) != 0) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: stat(%s) error: %s", parentExePath.c_str(),
                               std::strerror(errno));
        return setResult(AuthResult::StatFailed);
    }

    if (!S_ISREG(parentStat.st_mode)) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: '%s' is not a regular file", parentExePath.c_str());
        return setResult(AuthResult::NotRegularFile);
    }

    if (parentStat.st_mode & S_IWOTH) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: '%s' is world-writable", parentExePath.c_str());
        return setResult(AuthResult::WorldWritable);
    }

    struct stat selfStat {};
    if (stat(selfExePath.c_str(), &selfStat) == 0 && selfStat.st_uid == 0) {
        // System-installed helper (root owned): parent must also be root-owned or located in the same system binary
        // directory
        if (parentStat.st_uid != 0 && parentExePath.parent_path() != selfExePath.parent_path()) {
            KEYINJECTORD_LOG_ERROR("Launcher authorization failed: system helper at '%s' invoked by untrusted "
                                   "non-system executable at '%s'",
                                   selfExePath.c_str(), parentExePath.c_str());
            return setResult(AuthResult::UntrustedLocation);
        }
    } else {
        // Development / local build tree: parent binary must be owned by the current user or root
        if (parentStat.st_uid != getuid() && parentStat.st_uid != 0) {
            KEYINJECTORD_LOG_ERROR("Launcher authorization failed: parent binary '%s' UID %d != current UID %d",
                                   parentExePath.c_str(), parentStat.st_uid, getuid());
            return setResult(AuthResult::UntrustedLocation);
        }
    }

    KEYINJECTORD_LOG_INFO("Launcher authorization verified for peer PID %d (%s)", peerCreds.pid, parentExePath.c_str());
    return setResult(AuthResult::Success);
}

} // namespace keyinjectord
