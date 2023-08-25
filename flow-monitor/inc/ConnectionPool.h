#ifndef CONNECTIONPOOL_H_
#define CONNECTIONPOOL_H_
#include "Connection.h"
#include "Trackable.h"
#include <list>
#include <mutex>

class ConnectionPool : public Trackable<std::string, ConnectionPool *> {
public:
  ConnectionPool(std::string name, bool compress, std::vector<Connection *> &connections);
  virtual ~ConnectionPool();
  void pushConnection(Connection *connection, bool useTL = false);
  Connection *popConnection();
  Connection *popConnection(uint64_t size);
  int numConnections();

  uint64_t openFileOnAllServers();
  bool closeFileOnAllServers();
  static ConnectionPool *addNewConnectionPool(std::string filename, bool compress, std::vector<Connection *> &connections, bool &created);
  static bool removeConnectionPool(std::string filename, unsigned int dec = 1);
  static void removeAllConnectionPools();
  static std::unordered_map<std::string, uint64_t> *useCnt;
  static std::unordered_map<std::string, uint64_t> *consecCnt;
  static std::unordered_map<std::string, std::pair<double, double>> *stats;

private:
  void addOpenFileTask(Connection *server);
  bool openFileOnServer(Connection *server);

  std::string _name;
  uint64_t _fileSize;
  bool _compress;

  std::vector<Connection *> _connections;
  std::atomic_int _servers_requested;
  std::atomic_bool _valid_server;

  double _startTime;

  struct ConnectionCompare {
    double rate;
    double wRate;
    Connection *connection;

    ConnectionCompare() : rate(1000000000000),
                          wRate(0),
                          connection(NULL) {}

    ConnectionCompare(uint64_t bytes, uint64_t time, Connection *con) : rate(1000000000000),
                                                                        wRate(0),
                                                                        connection(con) {
      if (time) {
        rate = (bytes * 1000000000) / time;
        // wRate = rate; //* pow(0.95,con->consecutiveCnt());
        // rate=wRate;
        // std::cout<<"[MONITOR] "<<"r: "<<rate/1000000.0<<" wr: "<<wRate/1000000.0<<" "<<connection->port()<<std::endl;
      }
    }

    ConnectionCompare(const ConnectionCompare &entry) {
      rate = entry.rate;
      connection = entry.connection;
      wRate = entry.wRate;
    }

    ~ConnectionCompare() {}

    ConnectionCompare &operator=(const ConnectionCompare &other) {
      if (this != &other) {
        rate = other.rate;
        connection = other.connection;
        wRate = other.wRate;
      }
      return *this;
    }

    bool operator()(const ConnectionCompare &lhs, const ConnectionCompare &rhs) const {
      //bool ret = lhs.rate*pow(1.05,_curCnt-lhs.connection->consecutiveCnt()) < rhs.rate;
      bool ret = lhs.rate < rhs.rate;

      // if(rhs.connection->consecutiveCnt()>1){
      //     ret = lhs.rate < rhs.wRate;
      // }
      // std::cout<<"[MONITOR] "<<lhs.rate/1000000.0<<"MB/s "<<rhs.rate/1000000.0<<"MB/s "<<rhs.wRate/1000000.0<<"MB/s "<<lhs.connection->port()<<" "<<rhs.connection->port()<<" "<<ret<<std::endl;
      return ret;
    }
  };
  std::list<ConnectionCompare> _mq;
  std::mutex _qMutex;
  static std::atomic<uint64_t> _curCnt;
  std::unordered_map<Connection *, std::pair<double, double>> eta;
  std::unordered_map<Connection *, uint64_t> _prevStart;
  std::unordered_map<Connection *, double> _tmp;
};

#endif /* CONNECTIONPOOL_H_ */