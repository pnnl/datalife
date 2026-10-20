#include "MonitorPathPolicy.h"

#include <cstdlib>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

namespace {

thread_local unsigned int monitorInternalIODepth = 0;

std::string trimTrailingSeparators(std::string value) {
    while (value.size() > 1 && (value.back() == '/' || value.back() == '\\')) {
        value.pop_back();
    }
    return value;
}

std::string canonicalPath(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    char resolved[PATH_MAX];
    if (realpath(path.c_str(), resolved) != nullptr) {
        return trimTrailingSeparators(resolved);
    }

    std::string absolute = path;
    if (path.front() != '/') {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == nullptr) {
            return trimTrailingSeparators(path);
        }
        absolute = std::string(cwd) + "/" + path;
    }

    const std::string::size_type separator = absolute.find_last_of('/');
    const std::string parent = separator == 0
        ? "/"
        : absolute.substr(0, separator);
    const std::string leaf = absolute.substr(separator + 1);
    if (realpath(parent.c_str(), resolved) != nullptr) {
        return trimTrailingSeparators(std::string(resolved) + "/" + leaf);
    }
    return trimTrailingSeparators(absolute);
}

bool isUnderOutputRoot(const std::string& candidate, const std::string& outputRoot) {
    const std::string root = canonicalPath(outputRoot);
    const std::string path = canonicalPath(candidate);
    return !root.empty()
        && (path == root
            || (path.size() > root.size()
                && path.compare(0, root.size(), root) == 0
                && path[root.size()] == '/'));
}

}  // namespace

bool isMonitorInternalIO() {
    return monitorInternalIODepth != 0;
}

ScopedMonitorInternalIO::ScopedMonitorInternalIO() {
    ++monitorInternalIODepth;
}

ScopedMonitorInternalIO::~ScopedMonitorInternalIO() {
    if (monitorInternalIODepth != 0) {
        --monitorInternalIODepth;
    }
}

bool isMonitorOwnedPath(const char* path, const std::string& outputRoot) {
    if (path == nullptr || *path == '\0' || outputRoot.empty()) {
        return false;
    }

    return isUnderOutputRoot(path, outputRoot);
}

bool isMonitorOwnedPathAt(int dirfd, const char* path, const std::string& outputRoot) {
    if (path == nullptr || *path == '\0' || outputRoot.empty()) {
        return false;
    }
    if (*path == '/' || dirfd == AT_FDCWD) {
        return isMonitorOwnedPath(path, outputRoot);
    }

    const std::string descriptor = "/proc/self/fd/" + std::to_string(dirfd);
    char directory[PATH_MAX];
    const ssize_t length = readlink(descriptor.c_str(), directory, sizeof(directory) - 1);
    if (length < 0) {
        return false;
    }
    directory[length] = '\0';
    return isUnderOutputRoot(std::string(directory) + "/" + path, outputRoot);
}
