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

template <typename... Args>
void Receive(int socket, Args&... args);

template <typename... Args>
void Send(int socket, const Args&... args);

}  // namespace TCP