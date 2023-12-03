#include "basic-ops.hpp"

#include <fcntl.h>

#include <sstream>

namespace TCP {

TcpException::TcpException(ExceptionType type, int error)
    : type_(type), error_(error) {
  switch (type_) {
    case SocketCreation:
      s_what_ = "socket creation ";
      break;
    case Receiving:
      s_what_ = "receiving ";
      break;
    case ConnectionBreak:
      s_what_ = "connection break";
      break;
    case Sending:
      s_what_ = "sending ";
      break;
    case Binding:
      s_what_ = "binding ";
      break;
    case Listening:
      s_what_ = "listening ";
      break;
    case Acceptance:
      s_what_ = "acceptance ";
      break;
    case Connection:
      s_what_ = "connection ";
      break;
    case Setting:
      s_what_ = "setting ";
  }

  if (error_ != 0) {
    s_what_ += std::to_string(error_);
  }
}

const char* TcpException::what() const noexcept { return s_what_.c_str(); }
TcpException::ExceptionType TcpException::GetType() const noexcept {
  return type_;
}
int TcpException::GetErrno() const noexcept { return error_; }

const int kIntMaxDigitNum = 10;

bool IsAvailable(int socket) {
  int original_flags = fcntl(socket, F_GETFL, 0);
  if (original_flags < 0) {
    throw TcpException(TcpException::Setting, errno);
  }

  {
    int answ = fcntl(socket, F_SETFL, original_flags | O_NONBLOCK);
    if (answ < 0) {
      throw TcpException(TcpException::Setting, errno);
    }
  }

  char buffer;
  int answ = recv(socket, &buffer, 1, MSG_PEEK);
  if (answ == 0) {
    throw TcpException(TcpException::ConnectionBreak);
  }
  int curr_errno = errno;

  {
    int curr_flags = fcntl(socket, F_SETFL, original_flags);
    if (curr_flags < 0) {
      throw TcpException(TcpException::Setting, errno);
    }
  }
  if (answ > 0) {
    return true;
  }
  if (errno == EAGAIN) {
    return false;
  }
  throw TcpException(TcpException::Receiving, errno);
}

void ToArgs(std::stringstream& stream) {}
template <typename Head, typename... Tail>
void ToArgs(std::stringstream& stream, Head& head, Tail&... tail) {
  stream >> head;
  ToArgs(stream, tail...);
}

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

void FromArgs(std::string& output) {}
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