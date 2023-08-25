#include "MonitorFileStream.h"

MonitorFileStream::MonitorFileStream(int fd) : monitorFileDescriptor(fd) {
}

MonitorFileStream::~MonitorFileStream() {
}

bool MonitorFileStream::addStream(FILE *fp, int fd) {
    return Trackable<FILE *, MonitorFileStream *>::AddTrackable(
               fp, [=]() -> MonitorFileStream * {
                   return new MonitorFileStream(fd);
               }) != NULL;
}

bool MonitorFileStream::removeStream(FILE *fp) {
    return Trackable<FILE *, MonitorFileStream *>::RemoveTrackable(fp);
}

int MonitorFileStream::lookupStream(FILE *fp, ReaderWriterLock *&lock) {
    MonitorFileStream *v = Trackable<FILE *, MonitorFileStream *>::LookupTrackable(fp);
    if (v) {
        lock = &v->lock;
        return v->monitorFileDescriptor;
    }
    lock = NULL;
    return -1;
}

ReaderWriterLock *MonitorFileStream::getLock() {
    return &lock;
}

int MonitorFileStream::getFileDescriptor() {
    return monitorFileDescriptor;
}