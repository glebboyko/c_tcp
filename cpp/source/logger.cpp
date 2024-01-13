#include "logger.hpp"

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

}  // namespace TCP