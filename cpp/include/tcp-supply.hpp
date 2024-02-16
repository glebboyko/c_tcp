#pragma once

#include <chrono>
#include <concepts>
#include <exception>
#include <functional>
#include <optional>
#include <string>

namespace TCP {

enum MessagePriority { Error = 0, Warning = 1, Info = 2, Debug = 3 };
using logging_foo = std::function<void(const std::string&, const std::string&,
                                       const std::string&, int priority)>;

void LoggerCap(const std::string& l_module, const std::string& l_action,
               const std::string& l_event, int priority);

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
    NoData,

    Connection,

    IncomeChecking,

    Multithreading
  };

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
  enum LAction {
    FConstructor,
    FDestructor,
    FAccepter,
    FLoopAccepter,
    FCloseListener
  };

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
    FFromServerConstructor,
    FDestructor,
    FHeartBeatLoop,
    FSend,
    FRecv,
    FIsAvailable,
    FStopClient,
    FIsConnected
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

const int kULLMaxDigits = 20;

const int kDefPingThreshold = 1000;
const int kDefLoopPeriod = 100;

std::optional<int> WaitForData(int dp, int ms_timeout, Logger& logger,
                               logging_foo log_foo);
ssize_t RawSend(int dp, std::string message, size_t length) noexcept;
std::string RawRecv(int dp, size_t length) noexcept;

bool SetKeepIdle(int dp) noexcept;

template <typename T>
concept IOFriendly = requires(T val) {
  std::declval<std::stringstream>() >> val;
  std::declval<std::stringstream>() << val;
};

}  // namespace TCP