#include "tcp-client.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <list>
#include <string>

#include "basic-ops.hpp"

TcpClient::TcpClient(int protocol, int port, const char* server_addr) {
  connection_ = socket(AF_INET, SOCK_STREAM, 0);
  if (connection_ < 0) {
    throw std::ios_base::failure("cannot create socket " +
                                 std::to_string(errno));
  }

  sockaddr_in addr = {.sin_family = AF_INET,
                      .sin_port = htons(port),
                      .sin_addr = {inet_addr(server_addr)}};
  if (connect(connection_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    close(connection_);
    throw std::ios_base::failure("cannot connect " + std::to_string(errno));
  }
}

TcpClient::~TcpClient() { close(connection_); }

std::string TcpClient::Receive() { return ::Receive(connection_); }

bool TcpClient::Send(const std::string& message) {
  return ::Send(connection_, message);
}