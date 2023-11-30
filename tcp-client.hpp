#pragma once

#include <list>
#include <string>

class TcpClient {
 public:
  TcpClient(int protocol, int port, const char* server_addr);
  ~TcpClient();

  std::string Receive();
  bool Send(const std::string& message);

 private:
  int connection_;
};