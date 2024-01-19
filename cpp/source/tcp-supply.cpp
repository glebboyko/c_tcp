#include "tcp-supply.hpp"

#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <sstream>
#include <vector>

namespace TCP {

void LoggerCap(const std::string& l_module, const std::string& l_action,
               const std::string& l_event, int priority) {}

void Logger::Log(const std::string& event, int priority) {
  logger_(GetModule(), GetAction(), event, priority);
}

std::string GetAddress(void* pointer) {
  char address[64];
  sprintf(address, "%p", pointer);
  return address;
}

LServer::LServer(TCP::LServer::LAction action, void* pointer,
                 TCP::logging_foo logger)
    : action_(action), pointer_(pointer) {
  logger_ = logger;
}
std::string LServer::GetModule() const {
  return "TCP-SERVER " + GetAddress(pointer_);
}
std::string LServer::GetAction() const {
  switch (action_) {
    case FConstructor:
      return "CONSTRUCTOR";
    case FDestructor:
      return "DESTRUCTOR";
    case FAccepter:
      return "ACCEPTER";
    case FLoopAccepter:
      return "ACCEPTER LOOP";
    case FCloseListener:
      return "LISTENER CLOSER";
    default:
      return "CANNOT RECOGNIZE ACTION";
  }
}

LClient::LClient(TCP::LClient::LAction action, void* pointer,
                 TCP::logging_foo logger)
    : action_(action), pointer_(pointer) {
  logger_ = logger;
}
std::string LClient::GetModule() const {
  return "TCP-CLIENT " + GetAddress(pointer_);
}
std::string LClient::GetAction() const {
  switch (action_) {
    case FConstructor:
      return "CONSTRUCTOR";
    case FMoveConstructor:
      return "MOVE CONSTRUCTOR";
    case FFromServerConstructor:
      return "SERVER CONSTRUCTOR";
    case FDestructor:
      return "DESTRUCTOR";
    case FHeartBeatLoop:
      return "HEARTBEAT LOOP";
    case FSend:
      return "SENDER";
    case FRecv:
      return "RECEIVER";
    case FIsAvailable:
      return "AVAILABILITY CHECKER";
    case FStopClient:
      return "CONNECTION CLOSER";
    case FIsConnected:
      return "CONNECTION CHECKER";
    default:
      return "CANNOT RECOGNIZE ACTION";
  }
}

LException::LException(TCP::logging_foo logger) { logger_ = logger; }
std::string LException::GetModule() const { return "EXCEPTION"; }
std::string LException::GetAction() const { return "EXCEPTION"; }

TcpException::TcpException(ExceptionType type, logging_foo f_logger, int error,
                           bool message_leak)
    : type_(type), error_(error) {
  if (message_leak) {
    std::string mode = type_ == Receiving ? "received" : "sent";
    s_what_ = "The message could not be " + mode + " in full";
  } else {
    switch (type_) {
      case SocketCreation:
        s_what_ = "socket creation";
        break;
      case Receiving:
        s_what_ = "receiving";
        break;
      case ConnectionBreak:
        s_what_ = "connection break";
        break;
      case Sending:
        s_what_ = "sending";
        break;
      case Binding:
        s_what_ = "binding";
        break;
      case Listening:
        s_what_ = "listening";
        break;
      case Acceptance:
        s_what_ = "acceptance";
        break;
      case NoData:
        s_what_ = "no data";
        break;
      case Connection:
        s_what_ = "connection";
        break;
      case IncomeChecking:
        s_what_ = "income checking";
        break;
      case Multithreading:
        s_what_ = "multithreading";
    }
  }

  if (error_ != 0) {
    s_what_ += " " + std::to_string(error_);
  }

  LException(f_logger).Log(what(), Warning);
}

const char* TcpException::what() const noexcept { return s_what_.c_str(); }
TcpException::ExceptionType TcpException::GetType() const noexcept {
  return type_;
}
int TcpException::GetErrno() const noexcept { return error_; }

std::optional<int> WaitForData(int dp, int ms_timeout, Logger& logger,
                               logging_foo log_foo) {
  fd_set fd_set_v;
  FD_ZERO(&fd_set_v);
  FD_SET(dp, &fd_set_v);

  timeval timeout = {0, ms_timeout * 1'000};

  logger.Log("Starting waiting for data", Debug);
  auto time_start = std::chrono::system_clock::now();
  int answ = select(dp + 1, &fd_set_v, nullptr, nullptr, &timeout);
  auto time_stop = std::chrono::system_clock::now();
  logger.Log("Stopped waiting", Debug);

  if (answ < 0) {
    throw TcpException(TcpException::IncomeChecking, log_foo, errno);
  }

  if (answ != 0) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time_stop -
                                                                 time_start)
        .count();
  }
  return {};
}

ssize_t RawSend(int dp, std::string message, size_t length) noexcept {
  while (message.size() < length) {
    message.push_back('\0');
  }
  ssize_t answ = send(dp, message.c_str(), length, 0);
  return answ;
}
std::string RawRecv(int dp, size_t length) noexcept {
  std::vector<char> message(length);
  ssize_t answ = recv(dp, message.data(), length, 0);

  if (answ < 0) {
    return "";
  }

  std::string result;
  for (ssize_t i = 0; i < answ; ++i) {
    result.push_back(message[i]);
  }
  return result;
}

}  // namespace TCP