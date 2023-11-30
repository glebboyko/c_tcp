#include "basic-ops.hpp"

TcpException::TcpException(ExceptionType type, int error)
    : type_(type), error_(error) {
  switch (type_) {
    case SocketCreation:
      s_what_ = "socket creation ";
      break;
    case Receiving:
      s_what_ = "receiving ";
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
  }

  s_what_ += std::to_string(error_);
}

const char* TcpException::what() const noexcept { return s_what_.c_str(); }
TcpException::ExceptionType TcpException::GetType() const noexcept {
  return type_;
}
int TcpException::GetErrno() const noexcept { return error_; }