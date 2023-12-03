#pragma once

#include <sys/socket.h>

#include <exception>
#include <sstream>
#include <string>

namespace TCP {

class TcpException : public std::exception {
 public:
  enum ExceptionType {
    SocketCreation,
    Receiving,
    ConnectionBreak,
    Sending,

    Binding,
    Listening,
    Acceptance,

    Connection,

    Setting
  };

  TcpException(ExceptionType type, int error = 0);

  const char* what() const noexcept override;
  ExceptionType GetType() const noexcept;
  int GetErrno() const noexcept;

 private:
  ExceptionType type_;
  int error_;
  std::string s_what_;
};

bool IsAvailable(int socket);

void ToArgs(std::stringstream& stream);
template <typename Head, typename... Tail>
void ToArgs(std::stringstream& stream, Head& head, Tail&... tail) {
  stream >> head;
  ToArgs(stream, tail...);
}

const int kIntMaxDigitNum = 10;

template <typename... Args>
void Receive(int socket, Args&... args) {
  char num_buff[kIntMaxDigitNum + 1];
  // receive number of bytes to receive
  int b_recv = recv(socket, &num_buff, kIntMaxDigitNum + 1, 0);
  if (b_recv == 0) {
    throw TcpException(TcpException::ConnectionBreak);
  }
  if (b_recv < 0) {
    throw TcpException(TcpException::Receiving, errno);
  }
  int b_num;
  sscanf(num_buff, "%d", &b_num);

  // receive message
  char* buff = new char[b_num];
  b_recv = recv(socket, buff, b_num, 0);
  if (b_recv == 0) {
    delete[] buff;
    throw TcpException(TcpException::ConnectionBreak);
  }
  if (b_recv < 0) {
    delete[] buff;
    throw TcpException(TcpException::Receiving, errno);
  }

  std::stringstream stream;
  stream << buff;

  delete[] buff;

  ToArgs(stream, args...);
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
}

template <typename... Args>
void Send(int socket, const Args&... args) {
  std::string input;
  FromArgs(input, args...);
  // send number of bytes to send
  char num_buff[kIntMaxDigitNum + 1];
  for (int i = 0; i < kIntMaxDigitNum + 1; ++i) {
    num_buff[i] = '\0';
  }
  sprintf(num_buff, "%d", input.size() + 1);
  int b_sent = send(socket, num_buff, kIntMaxDigitNum + 1, 0);
  if (b_sent < 0) {
    if (errno == ECONNRESET) {
      throw TcpException(TcpException::ConnectionBreak);
    }
    throw TcpException(TcpException::Sending, errno);
  }

  // send message
  b_sent = send(socket, input.c_str(), input.size() + 1, 0);
  if (b_sent < 0) {
    if (errno == ECONNRESET) {
      throw TcpException(TcpException::ConnectionBreak);
    }
    throw TcpException(TcpException::Sending, errno);
  }
}

}  // namespace TCP