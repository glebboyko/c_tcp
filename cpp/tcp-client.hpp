#pragma once

#include <list>
#include <string>

#include "source/basic-ops.hpp"

namespace TCP {

class TcpClient {
 public:
  TcpClient(int protocol, int port, const char* server_addr);
  ~TcpClient();

  bool IsAvailable();

  template <typename... Args>
  void Receive(Args&... message) {
    return TCP::Receive(connection_, message...);
  }

  template <typename... Args>
  void Send(const Args&... message) {
    TCP::Send(connection_, message...);
  }

 private:
  int connection_;
};

}  // namespace TCP