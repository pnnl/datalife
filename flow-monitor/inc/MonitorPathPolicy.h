#ifndef DATALIFE_MONITOR_PATH_POLICY_H
#define DATALIFE_MONITOR_PATH_POLICY_H

#include <string>

// True while DataLife is emitting trace, statistic, or timer output on the
// current thread. Interposed I/O must bypass monitoring in this scope.
bool isMonitorInternalIO();

class ScopedMonitorInternalIO {
public:
    ScopedMonitorInternalIO();
    ~ScopedMonitorInternalIO();

    ScopedMonitorInternalIO(const ScopedMonitorInternalIO&) = delete;
    ScopedMonitorInternalIO& operator=(const ScopedMonitorInternalIO&) = delete;
};

// DataLife output is control-plane data and must never be monitored as
// workflow I/O.
bool isMonitorOwnedPath(const char* path, const std::string& outputRoot);
bool isMonitorOwnedPathAt(int dirfd, const char* path, const std::string& outputRoot);

#endif
