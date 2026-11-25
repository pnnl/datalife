#include "MonitorPathPolicy.h"

#include <cstdlib>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>

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

// The output root is the same for every check: resolve it once per thread
// (thread_local: no lock, so nothing to repair in a fork child).
const std::string& canonicalOutputRoot(const std::string& outputRoot) {
    thread_local std::string input;
    thread_local std::string resolved;
    thread_local bool valid = false;
    if (!valid || input != outputRoot) {
        input = outputRoot;
        resolved = canonicalPath(outputRoot);
        valid = true;
    }
    return resolved;
}

// realpath() walks every component of the path. A program opens many files in
// the same few directories (e.g. Python imports), so resolve each directory once
// per thread and append the leaf. The leaf itself is resolved with realpath()
// whenever it is a symlink, ".", or "..", so the result equals canonicalPath()
// unless a directory on the path is replaced by a symlink after first use.
std::string canonicalCandidate(const std::string& path) {
    if (path.empty()) {
        return path;
    }
    std::string absolute = path;
    if (path.front() != '/') {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == nullptr) {
            return canonicalPath(path);
        }
        absolute = std::string(cwd) + "/" + path;
    }
    const std::string::size_type separator = absolute.find_last_of('/');
    const std::string leaf = absolute.substr(separator + 1);
    if (leaf.empty() || leaf == "." || leaf == "..") {
        return canonicalPath(absolute);
    }
    struct stat st;
    if (lstat(absolute.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) {
        return canonicalPath(absolute);
    }
    const std::string parent = separator == 0 ? "/" : absolute.substr(0, separator);
    thread_local std::unordered_map<std::string, std::string> directories;
    auto found = directories.find(parent);
    if (found == directories.end()) {
        char resolved[PATH_MAX];
        if (realpath(parent.c_str(), resolved) == nullptr) {
            return canonicalPath(absolute);
        }
        if (directories.size() > 65536) {
            directories.clear();
        }
        found = directories.emplace(parent, trimTrailingSeparators(resolved)).first;
    }
    return trimTrailingSeparators(found->second == "/" ? "/" + leaf : found->second + "/" + leaf);
}

bool isUnderOutputRoot(const std::string& candidate, const std::string& outputRoot) {
    const std::string& root = canonicalOutputRoot(outputRoot);
    const std::string path = canonicalCandidate(candidate);
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

void enterMonitorInternalIO() {
    ++monitorInternalIODepth;
}

void leaveMonitorInternalIO() {
    if (monitorInternalIODepth != 0) {
        --monitorInternalIODepth;
    }
}

ScopedMonitorInternalIO::ScopedMonitorInternalIO() {
    enterMonitorInternalIO();
}

ScopedMonitorInternalIO::~ScopedMonitorInternalIO() {
    leaveMonitorInternalIO();
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
