#include "basic-ops.hpp"

#include <fcntl.h>

#include <sstream>

namespace TCP {

void LoggerCap(const std::string& l_module, const std::string& l_action,
               const std::string& l_event, int priority) {}

void Logger(LModule l_module, LAction l_action, const std::string& event,
            int priority, logging_foo logger,
            void* module_address) {
  std::string s_module;
  switch (l_module) {
    case CServer:
      s_module = "C-TCP SERVER";
      break;
    case CClient:
      s_module = "C-TCP CLIENT";
      break;
    case CExternFoo:
      s_module = "C-TCP EXTERN FOO";
      break;
    case CException:
      s_module = "C-TCP EXCEPTION";
      break;
    default:
      s_module = "CANNOT RECOGNIZE";
  }

  if (module_address != nullptr) {
    char address[64];
    sprintf(address, " %p", module_address);
    s_module += address;
  }

  std::string s_action;
  switch (l_action) {
    case FConstructor:
      s_action = "CONSTRUCTOR";
      break;
    case FMoveConstr:
      s_action = "MOVE CONSTRUCTOR";
      break;
    case FDestructor:
      s_action = "DESTRUCTOR";
      break;
    case FAcceptConnection:
      s_action = "CONNECTION ACCEPTANCE";
      break;
    case FCloseConnection:
      s_action = "CONNECTION CLOSURE";
      break;
    case FCloseListener:
      s_action = "LISTENER CLOSURE";
      break;
    case FIsAvailable:
      s_action = "CHECKING DATA EXISTENCE";
      break;
    case FReceive:
      s_action = "RECEIVING DATA";
      break;
    case FSend:
      s_action = "DATA SENDING";
      break;
    case FException:
      s_action = "EXCEPTION THROWING";
      break;
    default:
      s_action = "CANNOT RECOGNIZE";
  }

  logger(s_module, s_action, event, priority);
}

std::string LogSocket(int socket) {
  return "( Socket " + std::to_string(socket) + " )\t";
}

TcpException::TcpException(ExceptionType type, int error, bool message_leak)
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
}

TcpException::TcpException(ExceptionType type, logging_foo logger, int error,
                           bool message_leak)
    : TcpException(type, error, message_leak) {
  Logger(CException, FException, what(), Warning, logger);
}

const char* TcpException::what() const noexcept { return s_what_.c_str(); }
TcpException::ExceptionType TcpException::GetType() const noexcept {
  return type_;
}
int TcpException::GetErrno() const noexcept { return error_; }

bool IsAvailable(int socket, logging_foo logger) {
  auto log_socket = LogSocket(socket);
  Logger(CExternFoo, FIsAvailable,
         log_socket + "Trying to check if the data is available", Info, logger);

  Logger(CExternFoo, FIsAvailable, log_socket + "Trying to got flags", Debug,
         logger);
  int original_flags = fcntl(socket, F_GETFL, 0);
  if (original_flags < 0) {
    throw TcpException(TcpException::Setting, errno);
  }
  Logger(CExternFoo, FIsAvailable, log_socket + "Flags got", Debug, logger);

  {
    Logger(CExternFoo, FIsAvailable,
           log_socket + "Trying to set  NONBLOCK flag", Debug, logger);
    int answ = fcntl(socket, F_SETFL, original_flags | O_NONBLOCK);
    if (answ < 0) {
      throw TcpException(TcpException::Setting, errno);
    }
    Logger(CExternFoo, FIsAvailable, log_socket + "NONBLOCK flag set", Debug,
           logger);
  }

  Logger(CExternFoo, FIsAvailable, log_socket + "Trying to get data", Debug,
         logger);
  char buffer;
  int answ = recv(socket, &buffer, 1, MSG_PEEK);
  if (answ == 0) {
    throw TcpException(TcpException::ConnectionBreak);
  }
  int curr_errno = errno;
  Logger(CExternFoo, FIsAvailable, log_socket + "Tried to get data", Debug,
         logger);

  {
    Logger(CExternFoo, FIsAvailable,
           log_socket + "Trying to recover original flags", Debug, logger);
    int curr_flags = fcntl(socket, F_SETFL, original_flags);
    if (curr_flags < 0) {
      throw TcpException(TcpException::Setting, errno);
    }
    Logger(CExternFoo, FIsAvailable, log_socket + "original flags recovered",
           Debug, logger);
  }
  if (answ > 0) {
    Logger(CExternFoo, FIsAvailable, log_socket + "Data is available", Info,
           logger);

    return true;
  }
  if (errno == EAGAIN) {
    Logger(CExternFoo, FIsAvailable, log_socket + "Data is not available", Info,
           logger);

    return false;
  }
  throw TcpException(TcpException::Receiving, errno);
}

void ToArgs(std::stringstream& stream) {}
void FromArgs(std::string& output) {}

}  // namespace TCP