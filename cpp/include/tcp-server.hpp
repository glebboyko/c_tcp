#pragma once

#include <list>
#include <string>

#include "../source/basic-ops.hpp"
#include "tcp-supply.hpp"

namespace TCP {

class TcpServer {
 public:
  struct Client {
   private:
    Client() = default;
    Client(int descriptor) : dp_(descriptor) {}

    int dp_;
    friend TcpServer;
  };

  TcpServer(int protocol, int port, logging_foo logger,
            int max_queue_length = 1);
  TcpServer(int protocol, int port, int max_queue_length = 1);
  ~TcpServer();

  using ClientConnection = std::list<Client>::iterator;
  ClientConnection AcceptConnection();
  void CloseConnection(ClientConnection client);

  void CloseListener() noexcept;
  bool IsListenerOpen() const noexcept;

  bool IsAvailable(ClientConnection client);

  template <typename... Args>
  void Receive(ClientConnection client, Args&... message) {
    try {
      Logger(CServer, FReceive,
             LogSocket(client->dp_) + "Trying to receive data", Info, logger_,
             this);
      return TCP::Receive(client->dp_, logger_, message...);
    } catch (TcpException& tcp_exception) {
      if (tcp_exception.GetType() == TcpException::ConnectionBreak) {
        CloseConnection(client);
      }
      throw tcp_exception;
    }
  }

  template <typename... Args>
  void Send(ClientConnection client, const Args&... message) {
    try {
      Logger(CServer, FSend, LogSocket(client->dp_) + "Trying to send data",
             Info, logger_, this);
      TCP::Send(client->dp_, logger_, message...);
    } catch (TcpException& tcp_exception) {
      if (tcp_exception.GetType() == TcpException::ConnectionBreak) {
        CloseConnection(client);
      }
      throw tcp_exception;
    }
  }

 private:
  int listener_;
  std::list<Client> clients_;

  logging_foo logger_;
};

}  // namespace TCP