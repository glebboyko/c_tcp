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

std::string TcpClient::Receive() {
  std::string received;

  char buff[kRecvBuffSize];
  // receive number of bytes to receive
  int b_recv = recv(connection_, &buff, kRecvBuffSize, 0);
  if (b_recv <= 0) {
    throw std::ios_base::failure("cannot receive");
  }
  int b_num;
  sscanf(buff, "%d", &b_num);

  // receive message
  int b_received = 0;
  while (b_received < b_num) {
    b_recv = recv(connection_, &buff, kRecvBuffSize, 0);
    if (b_recv <= 0) {
      throw std::ios_base::failure("cannot receive");
    }
    b_received += b_recv;

    for (int i = 0; i < b_recv; ++i) {
      received.push_back(buff[i]);
    }
  }
  return received;
}

bool TcpClient::Send(const std::string& message) {
  // send number of bytes to send
  char num_of_bytes[sizeof(int) + 1];
  sprintf(num_of_bytes, "%d", message.size());
  int b_sent = send(connection_, num_of_bytes, sizeof(int) + 1, 0);
  if (b_sent < 0) {
    throw std::ios_base::failure("cannot send");
  }

  // send message
  b_sent = send(connection_, message.c_str(), message.size(), 0);
  if (b_sent < 0) {
    throw std::ios_base::failure("cannot send");
  }

  return b_sent == message.size();
}