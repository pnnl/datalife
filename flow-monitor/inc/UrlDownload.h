#ifndef URLDOWNLOAD_H
#define URLDOWNLOAD_H

#ifdef USE_CURL

#include <iostream>
#include <string.h>
#include <string>
#include <mutex>
#include <queue>
#include <curl/curl.h>
#include "ReaderWriterLock.h"

class CurlHandle {
private:
    CURL * _handle;
    
    static std::mutex _lock;
    static ReaderWriterLock _initLock;
    static std::queue<void*> _handles;
    static bool _started;

public:
    CurlHandle(std::string url);
    ~CurlHandle();
    template<class optType, class argType>
    void setOption(optType opt, argType arg);
    bool run();
    CURL * reset(std::string url);
    static bool startCurl();
    static bool endCurl(bool end=false);
    static void initCurl();
    static void destroyCurl();
};

/* Building a class that takes a url and a filepath and then downloads the file to the filepath.*/
class UrlDownload {
private:
    std::string _url;
    std::string _filepath;

    bool _supported;
    bool _exists;
    bool _ranges;
    unsigned int _size;

    char * getHeader();
    unsigned int checkHeaderForContentLength(char * data);
    bool checkHeaderForNotFound(char * data);
    bool checkHeaderForRanges(char * data);

public:
    UrlDownload(std::string url, int size = -1);
    ~UrlDownload();
    
    bool download();
    void * downloadRange(unsigned int start, unsigned int end);
    unsigned int size();
    std::string name();
    bool exists();
    bool ranges();

    
    static bool supportedType(std::string path);
    static std::string downloadUrl(std::string path);
    static int sizeUrl(std::string path);
    static bool checkDownloadable(std::string path);
    static void * downloadUrlRange(std::string path, unsigned int start, unsigned int end);
};

#define supportedUrlType(path) UrlDownload::supportedType(path)
#define checkUrlPath(path) UrlDownload::checkDownloadable(path)
#define sizeUrlPath(path) UrlDownload::sizeUrl(path)
#define downloadUrlPath(path) UrlDownload::downloadUrl(path)
#define downloadUrlRange(path, start, end) UrlDownload::downloadUrlRange(path, start, end)
#define curlEnd(x) (x) ? CurlHandle::endCurl(true) : false
#define curlInit CurlHandle::initCurl()
#define curlDestroy CurlHandle::destroyCurl()

#else

#define supportedUrlType(path) false
#define checkUrlPath(path) false
#define sizeUrlPath(path) -1
#define downloadUrlPath(path) path
#define downloadUrlRange(path, start, end) NULL
#define curlEnd(x) false 
#define curlInit
#define curlDestroy


#endif
#endif /* URLDOWNLOAD_H */

