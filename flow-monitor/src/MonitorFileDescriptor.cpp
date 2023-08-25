#include "MonitorFileDescriptor.h"

MonitorFileDescriptor::MonitorFileDescriptor(MonitorFile *file, unsigned int index) : _file(file),
                                                                             _index(index) {
}

MonitorFileDescriptor::~MonitorFileDescriptor() {
}

bool MonitorFileDescriptor::lookupMonitorFileDescriptor(int fd, MonitorFile *&file, unsigned int &index) {
    MonitorFileDescriptor *v = Trackable<int, MonitorFileDescriptor *>::LookupTrackable(fd);
    if (v) {
        file = v->_file;
        index = v->_index;
        /*if(file->type() == MonitorFile::Output) {
            OutputFile *outputFile = dynamic_cast<OutputFile*>(file);
            outputFile->setThreadFileDescriptor(fd);
        }*/
        return true;
    } 
    // else {
    //   printf("Returning false from Monitorfd where fd = %d", fd);
    // }

    return false;
}

bool MonitorFileDescriptor::addMonitorFileDescriptor(int fd, MonitorFile *file, unsigned int index) {
    return Trackable<int, MonitorFileDescriptor *>::AddTrackable(
               fd, [=]() -> MonitorFileDescriptor * {
                   return new MonitorFileDescriptor(file, index);
               }) != NULL;
}

bool MonitorFileDescriptor::removeMonitorFileDescriptor(int fd) {
    return Trackable<int, MonitorFileDescriptor *>::RemoveTrackable(fd);
}
