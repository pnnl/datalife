#include "HeldLocks.h"
#include "MonitorPathPolicy.h"

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <new>
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

// Every open() of a program asks whether its file is under the output
// directory. Resolving the two paths anew each time is a system call for every
// directory in each of them, so what does not change from one open to the next
// is kept: the output directory resolved, and the directories of the files a
// thread has asked about. Neither is looked at again: a directory that is
// replaced by another, or by a link to another, while the program runs is still
// taken for what it was.

// The output directory resolved, kept once it resolves. Not one given relative
// to the working directory, which a program may change. Written once, by the
// thread that takes `state` from 0 to 1, and read when `state` is 2.
struct ResolvedRoot {
    std::atomic<int> state;
    char given[PATH_MAX];
    char resolved[PATH_MAX];
};
ResolvedRoot resolvedRoot;

std::string canonicalRoot(const std::string& outputRoot) {
    if (resolvedRoot.state.load(std::memory_order_acquire) == 2 && outputRoot == resolvedRoot.given) {
        return resolvedRoot.resolved;
    }
    char resolved[PATH_MAX];
    if (outputRoot.empty() || outputRoot.front() != '/' || outputRoot.size() >= PATH_MAX
        || realpath(outputRoot.c_str(), resolved) == nullptr) {
        return canonicalPath(outputRoot);
    }
    const std::string root = trimTrailingSeparators(resolved);
    int unset = 0;
    if (resolvedRoot.state.compare_exchange_strong(unset, 1)) {
        memcpy(resolvedRoot.given, outputRoot.c_str(), outputRoot.size() + 1);
        memcpy(resolvedRoot.resolved, root.c_str(), root.size() + 1);
        resolvedRoot.state.store(2, std::memory_order_release);
    }
    return root;
}

// The directories of the files this thread has asked about, as the program
// spelled them (made absolute), and resolved. A program opens most of its files
// in a few directories, in turn. A map of the thread's own, so that no lock is
// taken; the flags beside it are plain data, which a thread has from its start
// to its very end:
//  - `busy` keeps a signal handler that opens a file while the thread is at
//    work in the map from using it half-changed;
//  - `gone` is set when the map is destroyed, at the thread's exit. What the
//    program still opens after that, from a destructor of its own, is resolved
//    without it.
typedef std::unordered_map<std::string, std::string> ResolvedDirectories;
__thread ResolvedDirectories* resolvedDirectories;
__thread bool resolvedDirectoriesBusy;
__thread bool resolvedDirectoriesGone;
const size_t maxResolvedDirectories = 4096; // then the map starts over

struct ResolvedDirectoriesOwner {
    ~ResolvedDirectoriesOwner() {
        resolvedDirectoriesGone = true;
        delete resolvedDirectories;
        resolvedDirectories = nullptr;
    }
};

// The directory resolved, from the map or by realpath(), which then goes into
// the map. False if it does not resolve.
bool resolveDirectory(const std::string& directory, std::string& resolved) {
    const bool usable = !resolvedDirectoriesBusy && !resolvedDirectoriesGone;
    if (usable && resolvedDirectories) {
        resolvedDirectoriesBusy = true;
        const auto found = resolvedDirectories->find(directory);
        const bool known = found != resolvedDirectories->end();
        if (known) {
            resolved = found->second;
        }
        resolvedDirectoriesBusy = false;
        if (known) {
            return true;
        }
    }
    char buffer[PATH_MAX];
    if (realpath(directory.c_str(), buffer) == nullptr) {
        return false;
    }
    resolved = buffer;
    if (usable) {
        resolvedDirectoriesBusy = true;
        try {
            if (!resolvedDirectories) {
                thread_local ResolvedDirectoriesOwner owner; // destroys the map at the thread's exit
                (void)owner;
                resolvedDirectories = new ResolvedDirectories();
            }
            if (resolvedDirectories->size() >= maxResolvedDirectories) {
                resolvedDirectories->clear();
            }
            (*resolvedDirectories)[directory] = resolved;
        } catch (...) {
            // out of memory: the directory is resolved again next time
        }
        resolvedDirectoriesBusy = false;
    }
    return true;
}

// canonicalPath() of a file a program opens, without resolving its directory
// again if the thread has opened a file in it before. What is left is the file's own
// name: one system call tells whether it is a link. Anything out of the
// ordinary (a link, "." or "..", a directory that does not resolve) goes to
// canonicalPath().
std::string canonicalOpened(const std::string& path) {
    std::string absolute = path;
    if (path.front() != '/') {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == nullptr) {
            return canonicalPath(path);
        }
        absolute = std::string(cwd) + "/" + path;
    }
    const std::string::size_type separator = absolute.find_last_of('/');
    const std::string parent = separator == 0 ? "/" : absolute.substr(0, separator);
    const std::string leaf = absolute.substr(separator + 1);
    if (leaf.empty() || leaf == "." || leaf == "..") {
        return canonicalPath(path);
    }
    // Not a link (EINVAL), or not there yet (ENOENT): its place is its
    // directory's, and its name.
    char target;
    if (readlink(absolute.c_str(), &target, 1) >= 0 || (errno != EINVAL && errno != ENOENT)) {
        return canonicalPath(path);
    }
    const bool exists = errno == EINVAL;
    std::string directory;
    if (!resolveDirectory(parent, directory)) {
        return canonicalPath(path);
    }
    // As canonicalPath() spells it, also in the root directory: a file that is
    // there as realpath() does, one that is not as the directory and the name.
    return trimTrailingSeparators(directory == "/" && exists ? "/" + leaf : directory + "/" + leaf);
}

// errno is left as it was: this is asked on the way into a call of the
// program's, which reads it after the call.
bool isUnderOutputRoot(const std::string& candidate, const std::string& outputRoot) {
    const int saved_errno = errno;
    const std::string root = canonicalRoot(outputRoot);
    const std::string path = canonicalOpened(candidate);
    errno = saved_errno;
    return !root.empty()
        && (path == root
            || (path.size() > root.size()
                && path.compare(0, root.size(), root) == 0
                && path[root.size()] == '/'));
}

}  // namespace

// Also true in a signal handler that interrupted this thread while it held one
// of the library's locks, or was taking one: a wrapper that waited for the lock
// there would wait for good, so the handler's calls go straight to the C library.
bool isMonitorInternalIO() {
    return monitorInternalIODepth != 0 || heldLocks() != 0;
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