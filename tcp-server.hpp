#pragma once

#include <list>
#include <string>

#include "basic-ops.hpp"

class TcpServer {
 public:
  struct Client {
   private:
    Client() = default;
    Client(int descriptor) : dp_(descriptor) {}

    int dp_;
    friend TcpServer;
  };

  TcpServer(int protocol, int port);
  ~TcpServer();

  std::list<Client>::iterator AcceptConnection();
  void CloseConnection(std::list<Client>::iterator client);

  std::string Receive(std::list<Client>::iterator client);
  void Send(std::list<Client>::iterator client, const std::string& message);

 private:
  int listener_;
  std::list<Client> clients_;
};