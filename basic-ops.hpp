#pragma once

#include <sys/socket.h>

#include <exception>
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

    Connection
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

std::string Receive(int socket);
void Send(int socket, const std::string& message);

}  // namespace TCP