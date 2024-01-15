#pragma once

#define BLOCK_SIZE 1024
#define MS_RECV_TIMEOUT 1000

#include <list>
#include <sstream>
#include <string>

#include "tcp-supply.hpp"

namespace TCP {

class TcpServer;

class TcpClient {
 public:
  TcpClient(const char* addr, int port, logging_foo f_logger);
  TcpClient(TcpClient&&);
  ~TcpClient();

  template <typename... Args>
  void Send(const Args&... args) {
    CleanTestQueries();

    std::string input;
    FromArgs(input, args...);
    RawSend(input);
  }

  template <typename... Args>
  bool Receive(int ms_timeout, Args&... args) {
    auto recv_str = RawRecv(ms_timeout);
    if (recv_str.empty()) {
      return false;
    }

    std::stringstream stream;
    stream << recv_str;
    ToArgs(stream, args...);
    return true;
  }

  bool IsAvailable();
  void CloseConnection() noexcept;
  bool IsConnected();

 private:
  int connection_;

  logging_foo logger_;

  TcpClient(int socket, logging_foo f_logger);

  void RawSend(const std::string&);
  std::string RawRecv(int ms_timeout);

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

  void CleanTestQueries();
  bool IsDataExist(int ms_timeout);

  friend TcpServer;
};

}  // namespace TCP