#pragma once

#include <map>
#include <mutex>
#include <queue>
#include <semaphore>
#include <thread>

#include "tcp-client.hpp"

namespace TCP {

class TcpServer {
 public:
  TcpServer(int port, logging_foo f_logger);
  TcpServer(int port);
  ~TcpServer();

  TcpClient AcceptConnection();

  void CloseListener() noexcept;
  bool IsListenerOpen() const noexcept;

 private:
  static const int kMaxClientLength = 1024;
  static const int kLoopMsTimeout = 100;

  int listener_;
  bool is_active_ = true;
  int port_;

  std::queue<TcpClient> accepted_;
  std::mutex accept_mutex_;
  std::counting_semaphore<kMaxClientLength> accepter_semaphore_ =
      std::counting_semaphore<kMaxClientLength>(0);

  std::map<uint64_t, int> uncomplete_client_;

  logging_foo logger_;

  std::thread accept_thread_;
  void AcceptLoop() noexcept;
};

}  // namespace TCP