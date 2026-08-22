#pragma once

#include <filesystem>
#include <optional>

#include <sys/types.h>

namespace keyinjectord {

enum class AuthResult {
    Success,
    InvalidFd,
    NotASocket,
    NotConnectedUnixSocket,
    PeerCredsFailed,
    UidMismatch,
    PidMismatch,
    ParentExeReadFailed,
    SelfExeReadFailed,
    UnauthorizedExecutable,
    StatFailed,
    NotRegularFile,
    WorldWritable,
    GroupWritable,
    NonRootOwner,
    UntrustedLocation,
    DeletedExecutable
};

const char* authResultToString(AuthResult result);

bool validateExecutableTopology(const std::filesystem::path& parentExePath, const std::filesystem::path& selfExePath,
                                AuthResult* outResult = nullptr);

bool authorizeLauncher(int socketFd, AuthResult* outResult = nullptr);

} // namespace keyinjectord
