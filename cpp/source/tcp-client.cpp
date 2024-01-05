#include "tcp-client.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <list>
#include <string>

namespace TCP {

TcpClient::TcpClient(int protocol, int port, const char* server_addr,
                     logging_foo logger)
    : logger_(logger) {
  Logger(CClient, FConstructor, "Trying to create socket", Debug, logger_,
         this);
  connection_ = socket(AF_INET, SOCK_STREAM, 0);
  if (connection_ < 0) {
    throw TcpException(TcpException::SocketCreation, errno);
  }
  Logger(CClient, FConstructor, "Socket created", Debug, logger_, this);

  Logger(CClient, FConstructor,
         "Trying to set connection to " + std::string(server_addr) + ":" +
             std::to_string(port),
         Info, logger_, this);
  sockaddr_in addr = {.sin_family = AF_INET,
                      .sin_port = htons(port),
                      .sin_addr = {inet_addr(server_addr)}};
  if (connect(connection_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    close(connection_);
    throw TcpException(TcpException::Connection, errno);
  }
  Logger(CClient, FConstructor, LogSocket(connection_) + "Connection set", Info,
         logger_, this);
}

TcpClient::TcpClient(TCP::TcpClient&& other)
    : connection_(other.connection_), logger_(other.logger_) {
  Logger(CClient, FMoveConstr,
         LogSocket(connection_) + "Moving from another client", Info, logger_,
         this);
  other.connection_ = 0;
}

TcpClient::TcpClient(int socket, TCP::logging_foo logger)
    : connection_(socket), logger_(logger) {
  Logger(CClient, FFromServerConstr, "Creating client from server", Info,
         logger_, this);
}

TcpClient::~TcpClient() {
  if (connection_ != 0) {
    close(connection_);
    Logger(CClient, FDestructor, LogSocket(connection_) + "Disconnected", Info,
           logger_, this);
  } else {
    Logger(CClient, FDestructor, "Already disconnected or moved", Info, logger_,
           this);
  }
}

void TcpClient::CloseConnection() noexcept {
  close(connection_);
  Logger(CClient, FCloseConnection, LogSocket(connection_) + "Disconnected",
         Info, logger_, this);
  connection_ = 0;
}
bool TcpClient::IsConnected() const noexcept { return connection_ != 0; }

bool TcpClient::IsAvailable() {
  Logger(CClient, FIsAvailable,
         LogSocket(connection_) + "Trying to check if the data is available",
         Info, logger_, this);
  return TCP::IsAvailable(connection_);
}

}  // namespace TCP