#include "tcp-server.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <list>
#include <string>

namespace TCP {

TcpServer::TcpServer(int protocol, int port) {
  listener_ = socket(AF_INET, SOCK_STREAM, protocol);
  if (listener_ < 0) {
    throw TcpException(TcpException::SocketCreation, errno);
  }

  sockaddr_in addr = {.sin_family = AF_INET,
                      .sin_port = htons(port),
                      .sin_addr = {htonl(INADDR_ANY)}};

  if (bind(listener_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    close(listener_);
    throw TcpException(TcpException::Binding, errno);
  }

  if (listen(listener_, 1) < 0) {
    close(listener_);
    throw TcpException(TcpException::Listening, errno);
  }
}

TcpServer::~TcpServer() {
  while (!clients_.empty()) {
    close(clients_.cbegin()->dp_);
    clients_.erase(clients_.cbegin());
  }
  close(listener_);
}

std::list<TcpServer::Client>::iterator TcpServer::AcceptConnection() {
  int client = accept(listener_, NULL, NULL);

  if (client < 0) {
    throw TcpException(TcpException::Acceptance, errno);
  }

  clients_.push_front(Client(client));
  return clients_.begin();
}

void TcpServer::CloseConnection(std::list<Client>::iterator client) {
  close(client->dp_);
  clients_.erase(client);
}

std::string TcpServer::Receive(std::list<Client>::iterator client) {
  try {
    return ::Receive(client->dp_);
  } catch (TcpException& tcp_exception) {
    if (tcp_exception.GetType() == TcpException::ConnectionBreak) {
      CloseConnection(client);
    }
    throw tcp_exception;
  }
}

void TcpServer::Send(std::list<Client>::iterator client,
                     const std::string& message) {
  ::Send(client->dp_, message);
}

}  // namespace TCP