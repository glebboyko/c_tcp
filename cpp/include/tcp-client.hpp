#pragma once

#include <list>
#include <string>

#include "../source/basic-ops.hpp"

namespace TCP {

class TcpClient {
 public:
  TcpClient(int protocol, int port, const char* server_addr,
            logging_foo logger = LoggerCap);
  ~TcpClient();

  void CloseConnection() noexcept;
  bool IsConnected() const noexcept;

  bool IsAvailable();

  template <typename... Args>
  void Receive(Args&... message) {
    Logger(CClient, FReceive, LogSocket(connection_) + "Trying to receive data",
           Info, logger_, this);
    return TCP::Receive(connection_, logger_, message...);
  }

  template <typename... Args>
  void Send(const Args&... message) {
    Logger(CClient, FSend, LogSocket(connection_) + "Trying to send data", Info,
           logger_, this);
    TCP::Send(connection_, logger_, message...);
  }

 private:
  int connection_;

  logging_foo logger_;
};

}  // namespace TCP