#pragma once

#include <list>
#include <mutex>
#include <string>

#include "../source/basic-ops.hpp"
#include "tcp-client.hpp"
#include "tcp-supply.hpp"

namespace TCP {

class TcpServer {
 public:
  TcpServer(int port, logging_foo f_logger,
            int max_queue_length = 1);
  TcpServer(int port, int max_queue_length = 1);
  ~TcpServer();

  TcpClient AcceptConnection();

  void CloseListener() noexcept;
  bool IsListenerOpen() const noexcept;

 private:
  int listener_;

  logging_foo logger_;
};

}  // namespace TCP