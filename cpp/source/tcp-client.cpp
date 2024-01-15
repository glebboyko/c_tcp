#include "tcp-client.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <list>
#include <string>
#include <vector>

namespace TCP {

TcpClient::TcpClient(const char* addr, int port, TCP::logging_foo f_logger)
    : logger_(f_logger) {
  connection_ = socket(AF_INET, SOCK_STREAM, 0);
  if (connection_ < 0) {
    throw TcpException(TcpException::SocketCreation, logger_, errno);
  }
  sockaddr_in addr_conf = {.sin_family = AF_INET,
                           .sin_port = htons(port),
                           .sin_addr = {inet_addr(addr)}};
  if (connect(connection_, (sockaddr*)&addr_conf, sizeof(addr)) < 0) {
    close(connection_);
    throw TcpException(TcpException::Connection, logger_, errno);
  }
}
TcpClient::TcpClient(TCP::TcpClient&& other)
    : connection_(other.connection_), logger_(other.logger_) {
  other.connection_ = 0;
}
TcpClient::TcpClient(int socket, TCP::logging_foo f_logger)
    : connection_(socket), logger_(f_logger) {}
TcpClient::~TcpClient() {
  if (connection_ != 0) {
    close(connection_);
  }
}

void CheckRecvAnsw(int answ, logging_foo logger) {
  if (answ > 0) {
    return;
  }
  if (errno == ECONNRESET) {
    throw TcpException(TcpException::ConnectionBreak, logger);
  }
  throw TcpException(TcpException::Receiving, logger, errno);
}
bool TcpClient::IsAvailable() {
  while (true) {
    if (!IsDataExist(0)) {
      return false;
    }
    char c_byte;
    CheckRecvAnsw(recv(connection_, &c_byte, 1, MSG_PEEK), logger_);
    if (c_byte == '1') {
      return true;
    }
    CheckRecvAnsw(recv(connection_, &c_byte, 1, 0), logger_);
  }
}

void TcpClient::CloseConnection() noexcept {
  close(connection_);
  connection_ = 0;
}

bool TcpClient::IsConnected() {
  CleanTestQueries();

  int test = send(connection_, "0", 1, 0);
  if (test > 0) {
    return true;
  }
  if (errno == ECONNRESET) {
    return false;
  }
  throw TcpException(TcpException::Sending, logger_, errno);
}

const int kIntMaxDigitNum = 20;

void CheckSendAnsw(int answ, logging_foo logger) {
  if (answ <= 0) {
    if (errno == ECONNRESET) {
      throw TcpException(TcpException::ConnectionBreak, logger);
    }
    throw TcpException(TcpException::Sending, logger, errno);
  }
}
void TcpClient::RawSend(const std::string& message) {
  CheckSendAnsw(send(connection_, "1", 1, 0), logger_);
  char num_buff[kIntMaxDigitNum + 1];
  for (int i = 0; i < kIntMaxDigitNum + 1; ++i) {
    num_buff[i] = '\0';
  }
  sprintf(num_buff, "%zu", message.size() + 1);
  int answ = send(connection_, num_buff, kIntMaxDigitNum + 1, 0);
  CheckSendAnsw(answ, logger_);
  if (answ < kIntMaxDigitNum + 1) {
    throw TcpException(TcpException::Sending, logger_, 0, true);
  }

  bool have_part = ((message.size() + 1) % BLOCK_SIZE) != 0;
  size_t num_of_blocks =
      ((message.size() + 1) / BLOCK_SIZE) + static_cast<size_t>(have_part);
  for (size_t i = 0; i < num_of_blocks; ++i) {
    int bytes_to_send = have_part && i == num_of_blocks - 1
                            ? (message.size() + 1) % BLOCK_SIZE
                            : BLOCK_SIZE;
    int answ =
        send(connection_, message.c_str() + (i * BLOCK_SIZE), bytes_to_send, 0);
    CheckSendAnsw(answ, logger_);
    if (answ < bytes_to_send) {
      throw TcpException(TcpException::Sending, logger_, 0, true);
    }
  }
}
std::string TcpClient::RawRecv(int ms_timeout) {
  while (true) {
    if (!IsDataExist(0) && !IsConnected()) {
      throw TcpException(TcpException::ConnectionBreak, logger_);
    }
    char mode;
    int answ = recv(connection_, &mode, 1, 0);
    CheckRecvAnsw(answ, logger_);
    if (mode == '1') {
      break;
    }
  }

  if (!IsDataExist(0) && !IsConnected()) {
    throw TcpException(TcpException::ConnectionBreak, logger_);
  }
  if (!IsDataExist(ms_timeout)) {
    return "";
  }
  char num_buff[kIntMaxDigitNum + 1];
  int answ = recv(connection_, &num_buff, kIntMaxDigitNum + 1, 0);
  CheckRecvAnsw(answ, logger_);
  if (answ < kIntMaxDigitNum + 1) {
    throw TcpException(TcpException::Receiving, logger_, 0, true);
  }
  size_t b_num = std::stoll(num_buff);

  std::vector<char> buff(b_num);
  bool have_part = (b_num % BLOCK_SIZE) != 0;
  size_t num_of_blocks = (b_num / BLOCK_SIZE) + static_cast<size_t>(have_part);
  for (size_t i = 0; i < num_of_blocks; ++i) {
    int bytes_to_send =
        have_part && i == num_of_blocks - 1 ? b_num % BLOCK_SIZE : BLOCK_SIZE;
    if (!IsDataExist(0) && !IsConnected()) {
      throw TcpException(TcpException::ConnectionBreak, logger_);
    }
    if (!IsDataExist(ms_timeout / num_of_blocks + num_of_blocks)) {
      return "";
    }
    int answ =
        recv(connection_, buff.data() + (i * BLOCK_SIZE), bytes_to_send, 0);
    CheckRecvAnsw(answ, logger_);
    if (answ < bytes_to_send) {
      throw TcpException(TcpException::Receiving, logger_, 0, true);
    }
  }
  return buff.data();
}

void TcpClient::ToArgs(std::stringstream& stream) {}
void TcpClient::FromArgs(std::string& output) {}

void TcpClient::CleanTestQueries() {
  while (true) {
    if (!IsDataExist(0)) {
      return;
    }
    char mode;
    CheckRecvAnsw(recv(connection_, &mode, 1, MSG_PEEK), logger_);
    if (mode != '0') {
      return;
    }
  }
}

bool TcpClient::IsDataExist(int ms_timeout) {
  fd_set fd_set_v;
  FD_ZERO(&fd_set_v);
  FD_SET(connection_, &fd_set_v);

  timeval timeout = {0, ms_timeout * 1'000};

  int answ = select(1, &fd_set_v, nullptr, nullptr, &timeout);
  if (answ < 0) {
    throw TcpException(TcpException::Receiving, logger_, errno);
  }
  return answ != 0;
}

}  // namespace TCP