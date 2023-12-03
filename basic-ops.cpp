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
void FromArgs(std::string& output) {}

}  // namespace TCP