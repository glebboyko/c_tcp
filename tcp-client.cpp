#include "tcp-client.hpp"

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <list>
#include <string>

TcpClient::TcpClient(int protocol, int port, unsigned int server_addr) {
  connection_ = socket(AF_INET, SOCK_STREAM, 0);
  if (connection_ < 0) {
    throw std::ios_base::failure("cannot create socket");
  }

  sockaddr_in addr = {.sin_family = AF_INET,
                      .sin_port = htons(port),
                      .sin_addr = {htonl(server_addr)}};
  if (connect(connection_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    close(connection_);
    throw std::ios_base::failure("cannot connect");
  }
}

TcpClient::~TcpClient() { close(connection_); }

std::string TcpClient::Receive(bool block) {
  std::string received;

  char buff[kRecvBuffSize];
  for (int i = 0;; ++i) {
    if (i != 0 || !block) {
      fcntl(connection_, F_SETFL, O_NONBLOCK);
    }
    int b_recv = recv(connection_, &buff, kRecvBuffSize, 0);
    if (i != 0 || !block) {
      fcntl(connection_, F_SETFL, ~O_NONBLOCK);
    }
    if (b_recv < 0) {
      throw std::ios_base::failure("cannot receive");
    }
    if (b_recv == 0) {
      break;
    }
    for (int i = 0; i < b_recv; ++i) {
      received.push_back(buff[i]);
    }
  }

  received.push_back('\0');
  return received;
}

bool TcpClient::Send(const std::string& message) {
  int b_sent = send(connection_, message.c_str(), message.size(), 0);
  if (b_sent < 0) {
    throw std::ios_base::failure("cannot send");
  }
  return b_sent == message.size();
}