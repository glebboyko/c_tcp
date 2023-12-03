#pragma once

#include <list>
#include <string>

#include "basic-ops.hpp"

namespace TCP {

class TcpClient {
 public:
  TcpClient(int protocol, int port, const char* server_addr);
  ~TcpClient();

  bool IsAvailable();

  template <typename... Args>
  void Receive(Args&... message);

  template <typename... Args>
  void Send(const Args&... message);

 private:
  int connection_;
};

}  // namespace TCP