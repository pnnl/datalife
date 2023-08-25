#ifndef RSOCKETADAPTER_H
#define RSOCKETADAPTER_H

#ifdef USE_RSOCKETS
#include <arpa/inet.h>
#include <netdb.h>
#include <rdma/rsocket.h>

#define SOCKETSENDFLAGS 0
//MSG_DONTWAIT
#else
#include "UnixIO.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SOCKETSENDFLAGS MSG_NOSIGNAL
#define rrecv(...) recv(__VA_ARGS__)
#define rsend(...) send(__VA_ARGS__)
#define rlisten(...) listen(__VA_ARGS__)
#define rpoll(...) poll(__VA_ARGS__)
#define rselect(...) select(__VA_ARGS__)
#define rbind(...) bind(__VA_ARGS__)
#define rclose(...) ((unixclose_t)dlsym(RTLD_NEXT, "close"))(__VA_ARGS__)
#define raccept(...) accept(__VA_ARGS__)
#define rconnect(...) connect(__VA_ARGS__)
#define rsocket(...) socket(__VA_ARGS__)
#define rsetsockopt(...) setsockopt(__VA_ARGS__)
#define rshutdown(...) shutdown(__VA_ARGS__)
#endif

#include <string>
#include <vector>
int getOutSocket(std::string name, unsigned int port);
int getInSocket(unsigned int port, int &socketFd);
int getInSocketOnInterface(const char * port, int &socketFd, const char *addr);

#endif /* RSOCKETADAPTER_H */
