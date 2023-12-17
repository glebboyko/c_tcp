#pragma once

#include <list>
#include <mutex>
#include <string>

#include "../source/basic-ops.hpp"
#include "tcp-supply.hpp"

namespace TCP {

class TcpServer {
 public:
  struct Client {
   public:
    Client(Client&& other);

   private:
    Client() = default;
    Client(int descriptor);

    int dp_;
    std::mutex using_socket_;
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
    if (!client->using_socket_.try_lock()) {
      Logger(CServer, FReceive, "Function is blocked by another thread",
             Warning, logger_, this);
      throw TcpException(TcpException::Multithreading, logger_);
    }

    try {
      Logger(CServer, FReceive,
             LogSocket(client->dp_) + "Trying to receive data", Info, logger_,
             this);
      TCP::Receive(client->dp_, logger_, message...);
      client->using_socket_.unlock();
    } catch (TcpException& tcp_exception) {
      client->using_socket_.unlock();
      if (tcp_exception.GetType() == TcpException::ConnectionBreak) {
        CloseConnection(client);
      }
      throw tcp_exception;
    }
  }

  template <typename... Args>
  void Send(ClientConnection client, const Args&... message) {
    if (!client->using_socket_.try_lock()) {
      Logger(CServer, FSend, "Function is blocked by another thread",
             Warning, logger_, this);
      throw TcpException(TcpException::Multithreading, logger_);
    }

    try {
      Logger(CServer, FSend, LogSocket(client->dp_) + "Trying to send data",
             Info, logger_, this);
      TCP::Send(client->dp_, logger_, message...);
      client->using_socket_.unlock();
    } catch (TcpException& tcp_exception) {
      client->using_socket_.unlock();
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