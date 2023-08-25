#ifndef FILECACHEREGISTER_H
#define FILECACHEREGISTER_H
#include "Loggable.h"
#include "ReaderWriterLock.h"
#include <atomic>
#include <string>

#define NUMENTRIES 10000
#define MAXFILENAME 1024

class FileCacheRegister {
  public:
    FileCacheRegister();
    ~FileCacheRegister();

    static FileCacheRegister *openFileCacheRegister(bool server=false);
    static void closeFileCacheRegister();

    unsigned int registerFile(std::string name);
    unsigned int getFileRegisterIndex(std::string name);

    void incUsage();
    void decUsage();
    void unlink();

  private:
    std::atomic_int _initFlag;
    std::atomic_int _available;

    ReaderWriterLock _registerLock;
    char _fileName[NUMENTRIES][MAXFILENAME];
    static std::atomic_int _registerInitLock;
    static FileCacheRegister *_register;
    static FileCacheRegister *initFileCacheRegister(bool server=false);
};

#endif /* FILECACHEREGISTER_H */
