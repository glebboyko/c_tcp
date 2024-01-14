#include "tcp-supply.hpp"

#include <fcntl.h>

#include <sstream>

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
    case FDestructor:
      return "DESTRUCTOR";
    case FSend:
      return "SENDER";
    case FBlockRecv:
      return "BLOCKING RECEIVER";
    case FNonBlockRecv:
      return "NONBLOCKING RECEIVER";
    case FIsAvailable:
      return "AVAILABILITY CHECKER";
    case FCloseConnection:
      return "CONNECTION CLOSER";
    case FIsConnected:
      return "CONNECTION CHECKER";
    case FSetFlags:
      return "FLAGS SETTER";
    case FRawSend:
      return "RAE SENDER";
    case FRawRecv:
      return "RAW RECEIVER";
    case FCleanTestQ:
      return "TEST QUERIES CLEANER";
    default:
      return "CANNOT RECOGNIZE ACTION";
  }
}

LException::LException(TCP::logging_foo logger) { logger_ = logger; }
std::string LException::GetModule() const { return "EXCEPTION"; }
std::string LException::GetAction() const { return "EXCEPTION"; }

std::string LogSocket(int socket) {
  return "( Socket " + std::to_string(socket) + " )\t";
}

TcpException::TcpException(ExceptionType type, logging_foo f_logger, int error,
                           bool message_leak)
    : type_(type), error_(error) {
  if (message_leak) {
    std::string mode = type_ == Receiving ? "received" : "sent";
    s_what_ = "The message could not be " + mode + " received in full";
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
      case Connection:
        s_what_ = "connection";
        break;
      case Setting:
        s_what_ = "setting";
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

}  // namespace TCP