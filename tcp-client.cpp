#include "tcp-client.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <list>
#include <string>

namespace TCP {

TcpClient::TcpClient(int protocol, int port, const char* server_addr) {
  connection_ = socket(AF_INET, SOCK_STREAM, 0);
  if (connection_ < 0) {
    throw TcpException(TcpException::SocketCreation, errno);
  }

  sockaddr_in addr = {.sin_family = AF_INET,
                      .sin_port = htons(port),
                      .sin_addr = {inet_addr(server_addr)}};
  if (connect(connection_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    close(connection_);
    throw TcpException(TcpException::Connection, errno);
  }
}

TcpClient::~TcpClient() { close(connection_); }

bool TcpClient::IsAvailable() { return TCP::IsAvailable(connection_); }

template <typename... Args>
void TcpClient::Receive(Args&... message) {
  return TCP::Receive(connection_, message...);
}

template <typename... Args>
void TcpClient::Send(const Args&... message) {
  TCP::Send(connection_, message...);
}

}  // namespace TCP