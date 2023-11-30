#pragma once

#include <list>
#include <string>

#include "basic-ops.hpp"

namespace TCP {

class TcpClient {
 public:
  TcpClient(int protocol, int port, const char* server_addr);
  ~TcpClient();

  std::string Receive();
  void Send(const std::string& message);

 private:
  int connection_;
};

}  // namespace TCP