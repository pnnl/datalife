#ifndef MONITORFILESTREAM_H
#define MONITORFILESTREAM_H

#include "ReaderWriterLock.h"
#include "Trackable.h"
#include <stdio.h>

class MonitorFileStream : public Trackable<FILE *, MonitorFileStream *> {
  public:
    static bool addStream(FILE *fp, int fd);
    static bool removeStream(FILE *fp);
    static int lookupStream(FILE *fp, ReaderWriterLock *&lock);

    ReaderWriterLock *getLock();
    int getFileDescriptor();

    MonitorFileStream(int fd);
    ~MonitorFileStream();

    static bool init;

  private:
    int monitorFileDescriptor;
    ReaderWriterLock lock;
};

#endif /* MONITORFILESTREAM_H */
