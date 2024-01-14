#pragma once

#include <exception>
#include <functional>
#include <string>

namespace TCP {

enum MessagePriority { Error = 0, Warning = 1, Info = 2, Debug = 3 };
using logging_foo = std::function<void(const std::string&, const std::string&,
                                       const std::string&, int priority)>;

void LoggerCap(const std::string& l_module, const std::string& l_action,
               const std::string& l_event, int priority);

std::string LogSocket(int socket);

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

    Setting,

    Multithreading
  };

  TcpException(ExceptionType type, int error = 0, bool message_leak = false);
  TcpException(ExceptionType type, logging_foo f_logger, int error = 0,
               bool message_leak = false);

  const char* what() const noexcept override;
  ExceptionType GetType() const noexcept;
  int GetErrno() const noexcept;

 private:
  ExceptionType type_;
  int error_;
  std::string s_what_;
};

class Logger {
 public:
  void Log(const std::string& event, int priority);

 protected:
  logging_foo logger_;

  virtual std::string GetModule() const = 0;
  virtual std::string GetAction() const = 0;
};

class LServer : public Logger {
 public:
  enum LAction { FConstructor, FDestructor, FAccepter, FCloseListener };

  LServer(LAction action, void* pointer, logging_foo logger);

 private:
  LAction action_;
  void* pointer_ = nullptr;

  std::string GetModule() const override;
  std::string GetAction() const override;
};
class LClient : public Logger {
 public:
  enum LAction {
    FConstructor,
    FMoveConstructor,
    FDestructor,
    FSend,
    FBlockRecv,
    FNonBlockRecv,
    FIsAvailable,
    FCloseConnection,
    FIsConnected,
    FSetFlags,
    FRawSend,
    FRawRecv,
    FCleanTestQ
  };

  LClient(LAction action, void* pointer, logging_foo logger);

 private:
  LAction action_;
  void* pointer_ = nullptr;

  std::string GetModule() const override;
  std::string GetAction() const override;
};
class LException : public Logger {
 public:
  LException(logging_foo logger);

 private:
  std::string GetModule() const override;
  std::string GetAction() const override;
};

}  // namespace TCP