#pragma once

#include <list>
#include <string>

#include "basic-ops.hpp"

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

  TcpServer(int protocol, int port, int max_queue_length = 1);
  ~TcpServer();

  std::list<Client>::iterator AcceptConnection();
  void CloseConnection(std::list<Client>::iterator client);

  bool IsAvailable(std::list<Client>::iterator client);

  template <typename... Args>
  void Receive(std::list<Client>::iterator client, Args&... message);

  template <typename... Args>
  void Send(std::list<Client>::iterator client, const Args&... message);

 private:
  int listener_;
  std::list<Client> clients_;
};

}  // namespace TCP