#pragma once

#include <list>
#include <string>

class TcpClient {
 public:
  TcpClient(int protocol, int port, unsigned int server_addr);
  ~TcpClient();

  std::string Receive(bool block = false);
  bool Send(const std::string& message);

 private:
  int connection_;

  const int kRecvBuffSize = 1024;
};