#pragma once

#include <exception>
#include <string>

class TcpException : public std::exception {
 public:
  enum ExceptionType {
    SocketCreation,
    Receiving,
    Sending,

    Binding,
    Listening,
    Acceptance,

    Connection
  };

  TcpException(ExceptionType type, int error);

  const char* what() const noexcept override;
  ExceptionType GetType() const noexcept;
  int GetErrno() const noexcept;

 private:
  ExceptionType type_;
  int error_;
  std::string s_what_;
};
