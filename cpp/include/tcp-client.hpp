#pragma once

#include <list>
#include <string>

#include "../source/basic-ops.hpp"
#include "tcp-supply.hpp"

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
    try {
      return TCP::Receive(connection_, logger_, message...);
    } catch (TcpException& exception) {
      if (exception.GetType() == TcpException::ConnectionBreak) {
        Logger(CClient, FReceive,
               LogSocket(connection_) + "Connection broke. Trying to close",
               Info, logger_, this);
        CloseConnection();
      }
      throw exception;
    }
  }

  template <typename... Args>
  void Send(const Args&... message) {
    Logger(CClient, FSend, LogSocket(connection_) + "Trying to send data", Info,
           logger_, this);
    try {
      TCP::Send(connection_, logger_, message...);
    } catch (TcpException& exception) {
      if (exception.GetType() == TcpException::ConnectionBreak) {
        Logger(CClient, FSend,
               LogSocket(connection_) + "Connection broke. Trying to close",
               Info, logger_, this);
        CloseConnection();
      }
      throw exception;
    }
  }

 private:
  int connection_;

  logging_foo logger_;
};

}  // namespace TCP