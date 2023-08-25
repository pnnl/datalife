#ifndef MONITORFILEDESCRIPTOR_H
#define MONITORFILEDESCRIPTOR_H

#include "MonitorFile.h"
#include "Trackable.h"
//#include "OutputFile.h"

class MonitorFileDescriptor : public Trackable<int, MonitorFileDescriptor *> {
  public:
    static bool lookupMonitorFileDescriptor(int fd, MonitorFile *&file, unsigned int &index);
    static bool addMonitorFileDescriptor(int fd, MonitorFile *file, unsigned int index);
    static bool removeMonitorFileDescriptor(int fd);
    MonitorFile* getFile() {return _file;}
    MonitorFileDescriptor(MonitorFile *file, unsigned int index);
    ~MonitorFileDescriptor();

  private:
    MonitorFile *_file;
    int _index;
};

#endif /* MONITORFILEDESCRIPTOR_H */
