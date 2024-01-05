#include "tcp-server.hpp"

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <list>
#include <string>

namespace TCP {

TcpServer::TcpServer(int protocol, int port, logging_foo logger,
                     int max_queue_length)
    : logger_(logger) {
  Logger(CServer, FConstructor, "Trying to create socket", Debug, logger_,
         this);
  listener_ = socket(AF_INET, SOCK_STREAM, protocol);
  if (listener_ < 0) {
    throw TcpException(TcpException::SocketCreation, errno);
  }
  Logger(CServer, FConstructor, "Socket created ", Debug, logger_, this);

  sockaddr_in addr = {.sin_family = AF_INET,
                      .sin_port = htons(port),
                      .sin_addr = {htonl(INADDR_ANY)}};

  Logger(CServer, FConstructor, "Trying to bind to " + std::to_string(port),
         Info, logger_, this);
  if (bind(listener_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    close(listener_);
    throw TcpException(TcpException::Binding, errno);
  }
  Logger(CServer, FConstructor, "Bound", Info, logger_, this);

  Logger(CServer, FConstructor, "Trying to listen", Info, logger_, this);
  if (listen(listener_, max_queue_length) < 0) {
    close(listener_);
    throw TcpException(TcpException::Listening, errno);
  }
  Logger(CServer, FConstructor, "Listening", Info, logger_, this);
}
TcpServer::TcpServer(int protocol, int port, int max_queue_length)
    : TcpServer(protocol, port, LoggerCap, max_queue_length) {}

TcpServer::~TcpServer() {
  if (listener_ != 0) {
    close(listener_);
    Logger(CServer, FDestructor, "Stopped listening", Info, logger_, this);
  } else {
    Logger(CServer, FDestructor, "Listening has already been stopped", Info,
           logger_, this);
  }
}

TcpClient TcpServer::AcceptConnection() {
  Logger(CServer, FAcceptConnection, "Trying to accept connection", Info,
         logger_, this);
  int client = accept(listener_, NULL, NULL);

  if (client < 0) {
    if (errno == ECONNABORTED) {
      throw TcpException(TcpException::ConnectionBreak);
    }
    throw TcpException(TcpException::Acceptance, errno);
  }


  Logger(CServer, FAcceptConnection, LogSocket(client) + "Connection accepted",
         Info, logger_, this);
  return TcpClient(client, logger_);
}

void TcpServer::CloseListener() noexcept {
  close(listener_);
  Logger(CServer, FCloseListener, "Listener closed", Info, logger_, this);
  listener_ = 0;
}
bool TcpServer::IsListenerOpen() const noexcept { return listener_ != 0; }

}  // namespace TCP