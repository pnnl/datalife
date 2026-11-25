#include "MonitorFileDescriptor.h"

std::atomic<uint64_t> MonitorFileDescriptor::_monitored[MonitorFileDescriptor::kExact / 64];

MonitorFileDescriptor::MonitorFileDescriptor(MonitorFile *file, unsigned int index) : _file(file),
                                                                             _index(index) {
}

MonitorFileDescriptor::~MonitorFileDescriptor() {
}

void MonitorFileDescriptor::setMonitored(int fd, bool monitored) {
    if (fd >= 0 && fd < kExact) {
        const uint64_t bit = (uint64_t)1 << (fd % 64);
        if (monitored) {
            _monitored[fd / 64].fetch_or(bit);
        } else {
            _monitored[fd / 64].fetch_and(~bit);
        }
    }
}

bool MonitorFileDescriptor::useMonitorFileDescriptor(int fd, MonitorFile *&file, unsigned int &index) {
    return mayBeMonitored(fd) &&
           Trackable<int, MonitorFileDescriptor *>::WithTrackable(fd, [&](MonitorFileDescriptor *v) {
               file = v->_file;
               index = v->_index;
               file->incUsers();
           });
}

MonitorFile *MonitorFileDescriptor::replaceMonitorFileDescriptor(int fd, MonitorFile *file, unsigned int index) {
    if (!file && !mayBeMonitored(fd)) {
        return NULL;
    }
    MonitorFileDescriptor *old = Trackable<int, MonitorFileDescriptor *>::ReplaceTrackable(
        fd, file ? new MonitorFileDescriptor(file, index) : NULL, [=]() { setMonitored(fd, file != NULL); });
    if (!old) {
        return NULL;
    }
    MonitorFile *oldFile = old->_file;
    oldFile->releaseFilePosIndex(old->_index); // the file stays: the caller has the descriptor's use of it
    delete old;
    return oldFile;
}

void MonitorFileDescriptor::closeMonitorFile(MonitorFile *file, int openAs) {
    if (!file) {
        return;
    }
    if (file->traced()) {
        file->close(openAs);
    }
    MonitorFile::removeMonitorFile(file);
}

bool MonitorFileDescriptor::addMonitorFileDescriptor(int fd, MonitorFile *file, unsigned int index) {
    closeMonitorFile(replaceMonitorFileDescriptor(fd, file, index), -1);
    return true;
}

bool MonitorFileDescriptor::removeMonitorFileDescriptor(int fd, int openAs) {
    MonitorFile *file = replaceMonitorFileDescriptor(fd, NULL, 0);
    closeMonitorFile(file, openAs);
    return file != NULL;
}