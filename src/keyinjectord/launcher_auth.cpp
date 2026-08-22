#include "launcher_auth.h"

#include "logging.h"

#include <cerrno>
#include <climits>
#include <cstring>
#include <filesystem>
#include <string>

#include <sys/capability.h>
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
            return "Failed to stat parent executable binary or directory";
        case AuthResult::NotRegularFile:
            return "Parent executable is not a regular file";
        case AuthResult::WorldWritable:
            return "Parent executable or directory is world-writable";
        case AuthResult::GroupWritable:
            return "Parent executable or directory is group-writable";
        case AuthResult::NonRootOwner:
            return "Production helper executable, parent binary, or directory is not owned by root (UID 0)";
        case AuthResult::UntrustedLocation:
            return "Parent executable is located in an untrusted or non-colocated directory";
        case AuthResult::DeletedExecutable:
            return "Executable binary was unlinked or deleted from disk";
    }
    return "Unknown error";
}

bool validateExecutableTopology(const std::filesystem::path& parentExePath, const std::filesystem::path& selfExePath,
                                AuthResult* outResult) {
    auto setResult = [outResult](AuthResult res) {
        if (outResult) {
            *outResult = res;
        }
        return res == AuthResult::Success;
    };

    std::string parentStr = parentExePath.string();
    std::string selfStr = selfExePath.string();

    if (parentStr.empty() || selfStr.empty()) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: empty executable path");
        return setResult(AuthResult::UntrustedLocation);
    }

    if (parentStr.ends_with(" (deleted)") || selfStr.ends_with(" (deleted)")) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: executable is deleted (parent: '%s', self: '%s')",
                               parentStr.c_str(), selfStr.c_str());
        return setResult(AuthResult::DeletedExecutable);
    }

    std::string parentNorm = parentExePath.lexically_normal().string();

    // Reject well-known untrusted/temporary/shared locations
    if (parentNorm == "/tmp" || parentNorm.starts_with("/tmp/") || parentNorm == "/var/tmp" ||
        parentNorm.starts_with("/var/tmp/") || parentNorm == "/dev/shm" || parentNorm.starts_with("/dev/shm/") ||
        parentNorm == "/run/user" || parentNorm.starts_with("/run/user/")) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: parent binary in untrusted directory '%s'",
                               parentNorm.c_str());
        return setResult(AuthResult::UntrustedLocation);
    }

    std::string parentExeName = parentExePath.filename().string();
#ifdef KEYINJECTORD_DEV_AUTH
    if (parentExeName != "qtranscribe" && parentExeName != "test_ipc_server" &&
        parentExeName != "test_transcription_pipeline") {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: unauthorized parent executable name '%s' at '%s'",
                               parentExeName.c_str(), parentExePath.c_str());
        return setResult(AuthResult::UnauthorizedExecutable);
    }
#else
    if (parentExeName != "qtranscribe") {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: unauthorized parent executable name '%s' at '%s'",
                               parentExeName.c_str(), parentExePath.c_str());
        return setResult(AuthResult::UnauthorizedExecutable);
    }
#endif

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

#ifndef KEYINJECTORD_DEV_AUTH
    if ((parentStat.st_mode & S_IWGRP) && parentStat.st_gid != 0) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: '%s' is group-writable by non-root group %d",
                               parentExePath.c_str(), parentStat.st_gid);
        return setResult(AuthResult::GroupWritable);
    }
#endif

    struct stat selfStat {};
    if (stat(selfExePath.c_str(), &selfStat) != 0) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: stat(%s) error: %s", selfExePath.c_str(),
                               std::strerror(errno));
        return setResult(AuthResult::StatFailed);
    }

    if (!S_ISREG(selfStat.st_mode)) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: '%s' is not a regular file", selfExePath.c_str());
        return setResult(AuthResult::NotRegularFile);
    }

    if (selfStat.st_mode & S_IWOTH) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: '%s' is world-writable", selfExePath.c_str());
        return setResult(AuthResult::WorldWritable);
    }

#ifndef KEYINJECTORD_DEV_AUTH
    if ((selfStat.st_mode & S_IWGRP) && selfStat.st_gid != 0) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: '%s' is group-writable by non-root group %d",
                               selfExePath.c_str(), selfStat.st_gid);
        return setResult(AuthResult::GroupWritable);
    }
#endif

    std::filesystem::path parentDir = parentExePath.parent_path().lexically_normal();
    std::filesystem::path selfDir = selfExePath.parent_path().lexically_normal();

    struct stat parentDirStat {};
    if (stat(parentDir.c_str(), &parentDirStat) != 0) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: stat directory (%s) error: %s", parentDir.c_str(),
                               std::strerror(errno));
        return setResult(AuthResult::StatFailed);
    }

    if (parentDirStat.st_mode & S_IWOTH) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: parent directory '%s' is world-writable",
                               parentDir.c_str());
        return setResult(AuthResult::WorldWritable);
    }

#ifndef KEYINJECTORD_DEV_AUTH
    if ((parentDirStat.st_mode & S_IWGRP) && parentDirStat.st_gid != 0) {
        KEYINJECTORD_LOG_ERROR(
            "Launcher authorization failed: parent directory '%s' is group-writable by non-root group %d",
            parentDir.c_str(), parentDirStat.st_gid);
        return setResult(AuthResult::GroupWritable);
    }
#endif

    struct stat selfDirStat {};
    if (stat(selfDir.c_str(), &selfDirStat) != 0) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: stat directory (%s) error: %s", selfDir.c_str(),
                               std::strerror(errno));
        return setResult(AuthResult::StatFailed);
    }

    if (selfDirStat.st_mode & S_IWOTH) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: self directory '%s' is world-writable", selfDir.c_str());
        return setResult(AuthResult::WorldWritable);
    }

#ifndef KEYINJECTORD_DEV_AUTH
    if ((selfDirStat.st_mode & S_IWGRP) && selfDirStat.st_gid != 0) {
        KEYINJECTORD_LOG_ERROR(
            "Launcher authorization failed: self directory '%s' is group-writable by non-root group %d",
            selfDir.c_str(), selfDirStat.st_gid);
        return setResult(AuthResult::GroupWritable);
    }
#endif

    // Colocation validation:
    // Both executables must be directly colocated in the exact same directory, or be the exact same file (self-test).
    if (parentExePath != selfExePath && parentDir != selfDir) {
        KEYINJECTORD_LOG_ERROR(
            "Launcher authorization failed: parent path '%s' is not colocated in helper directory '%s'",
            parentExePath.c_str(), selfExePath.c_str());
        return setResult(AuthResult::UntrustedLocation);
    }

    // Verify ownership:
#ifdef KEYINJECTORD_DEV_AUTH
    if (selfStat.st_uid != 0) {
        // User-owned helper in development / test mode: parent binary and directory must belong to current user or root
        if ((parentStat.st_uid != getuid() && parentStat.st_uid != 0) ||
            (parentDirStat.st_uid != getuid() && parentDirStat.st_uid != 0)) {
            KEYINJECTORD_LOG_ERROR(
                "Dev launcher authorization failed: parent UID (%d) / dir UID (%d) mismatch with current UID (%d)",
                parentStat.st_uid, parentDirStat.st_uid, getuid());
            return setResult(AuthResult::UntrustedLocation);
        }
        KEYINJECTORD_LOG_WARN("Dev launcher authorization accepted for user-owned binary in development mode");
        return setResult(AuthResult::Success);
    }
#endif

    // In production / release mode (or when self is root-owned):
    // Helper, parent binary, and containing directories must be strictly owned by root (UID 0).
    if (selfStat.st_uid != 0 || parentStat.st_uid != 0 || parentDirStat.st_uid != 0 || selfDirStat.st_uid != 0) {
        KEYINJECTORD_LOG_ERROR("Launcher authorization failed: production helper/parent/dir must be root-owned (self "
                               "UID: %d, parent UID: %d, parent dir UID: %d, self dir UID: %d)",
                               selfStat.st_uid, parentStat.st_uid, parentDirStat.st_uid, selfDirStat.st_uid);
        return setResult(AuthResult::NonRootOwner);
    }

    return setResult(AuthResult::Success);
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

    if (!validateExecutableTopology(parentExePath, selfExePath, outResult)) {
        return false;
    }

    KEYINJECTORD_LOG_INFO("Launcher authorization verified for peer PID %d (%s)", peerCreds.pid, parentExePath.c_str());
    return setResult(AuthResult::Success);
}

} // namespace keyinjectord
