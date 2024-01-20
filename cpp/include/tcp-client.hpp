#pragma once

#define BLOCK_SIZE 1024
#define MS_RECV_TIMEOUT 1000

#include <list>
#include <mutex>
#include <optional>
#include <queue>
#include <semaphore>
#include <sstream>
#include <string>
#include <thread>

#include "tcp-supply.hpp"

namespace TCP {

class TcpServer;

class TcpClient {
 public:
  TcpClient(const char* addr, int port, logging_foo f_logger = LoggerCap);
  TcpClient(TcpClient&&) noexcept;
  ~TcpClient();

  template <typename... Args>
  void Send(const Args&... args) {
    LClient logger(LClient::FSend, this, logger_);
    logger.Log("Starting sending method", Debug);

    logger.Log("Getting string from args", Debug);
    std::string input;
    FromArgs(input, args...);
    logger.Log("Sending message", Debug);
    StrSend(input, logger);
    logger.Log("Message sent", Info);
  }

  template <typename... Args>
  bool Receive(int ms_timeout, Args&... args) {
    LClient logger(LClient::FRecv, this, logger_);
    logger.Log("Starting receiving method", Debug);

    logger.Log("Receiving string", Debug);
    auto recv_str = StrRecv(ms_timeout, logger);
    logger.Log(
        "Method returned string of size " + std::to_string(recv_str.size()),
        Debug);
    if (recv_str.empty()) {
      return false;
    }

    logger.Log("Setting args from string", Debug);
    std::stringstream stream;
    stream << recv_str;
    ToArgs(stream, args...);
    logger.Log("Message received", Info);
    return true;
  }

  std::string StrRecv(int ms_timeout, Logger& logger);
  void StrSend(const std::string& message, Logger& logger);

  void StopClient() noexcept;
  bool IsAvailable();
  bool IsConnected() noexcept;

  int GetPing();

 private:
  int main_socket_;
  int heartbeat_socket_;

  std::thread heartbeat_thread_;

  TcpClient** this_pointer_ = nullptr;
  std::mutex* this_mutex_ = nullptr;

  bool is_active_ = true;

  int ms_ping_ = 0;

  logging_foo logger_;

  TcpClient(int heartbeat_socket, int main_socket, logging_foo f_logger);

  static void HeartBeatClient(TcpClient** this_pointer,
                        std::mutex* this_mutex) noexcept;
  static void HeartBeatServer(TcpClient** this_pointer,
                              std::mutex* this_mutex) noexcept;

  void ToArgs(std::stringstream& stream);
  template <typename Head, typename... Tail>
  void ToArgs(std::stringstream& stream, Head& head, Tail&... tail) {
    stream >> head;
    if (stream.eof()) {
      return;
    }
    ToArgs(stream, tail...);
  }
  void FromArgs(std::string& output);
  template <typename Head, typename... Tail>
  void FromArgs(std::string& output, const Head& head, const Tail&... tail) {
    std::stringstream stream;
    stream << head;
    std::string str_head;
    stream >> str_head;

    if (!output.empty()) {
      output.push_back(' ');
    }
    output += str_head;

    FromArgs(output, tail...);
  }

  friend TcpServer;
};

}  // namespace TCP