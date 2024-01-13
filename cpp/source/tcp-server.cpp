#include "tcp-server.hpp"

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <list>
#include <string>

#include "basic-ops.hpp"
#include "logger.hpp"

namespace TCP {

TcpServer::TcpServer(int port, logging_foo f_logger, int max_queue_length)
    : logger_(f_logger) {
  LServer logger(LServer::FConstructor, this, logger_);

  logger.Log("Trying to create socket", Debug);
  listener_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listener_ < 0) {
    throw TcpException(TcpException::SocketCreation, logger_, errno);
  }
  logger.Log("Socket created ", Debug);

  sockaddr_in addr = {.sin_family = AF_INET,
                      .sin_port = htons(port),
                      .sin_addr = {htonl(INADDR_ANY)}};

  logger.Log("Trying to bind to " + std::to_string(port), Info);
  if (bind(listener_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    close(listener_);
    throw TcpException(TcpException::Binding, logger_, errno);
  }
  logger.Log("Bound", Info);

  logger.Log("Trying to listen", Info);
  if (listen(listener_, max_queue_length) < 0) {
    close(listener_);
    throw TcpException(TcpException::Listening, logger_, errno);
  }
  logger.Log("Listening", Info);
}
TcpServer::TcpServer(int port, int max_queue_length)
    : TcpServer(port, LoggerCap, max_queue_length) {}

TcpServer::~TcpServer() {
  LServer logger(LServer::FDestructor, this, logger_);
  if (listener_ != 0) {
    close(listener_);
    logger.Log("Stopped listening", Info);
  } else {
    logger.Log("Listening has already been stopped", Info);
  }
}

TcpClient TcpServer::AcceptConnection() {
  LServer logger(LServer::FAccepter, this, logger_);

  logger.Log("Trying to accept connection", Info);
  int client = accept(listener_, NULL, NULL);

  if (client < 0) {
    if (errno == ECONNABORTED) {
      throw TcpException(TcpException::ConnectionBreak, logger_);
    }
    throw TcpException(TcpException::Acceptance, logger_, errno);
  }

  logger.Log(LogSocket(client) + "Connection accepted", Info);
  return TcpClient(client, logger_);
}

void TcpServer::CloseListener() noexcept {
  LServer logger(LServer::FCloseListener, this, logger_);

  close(listener_);
  logger.Log("Listener closed", Info);
  listener_ = 0;
}
bool TcpServer::IsListenerOpen() const noexcept { return listener_ != 0; }

}  // namespace TCP