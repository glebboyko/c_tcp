#include "tcp-client.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <list>
#include <string>
#include <vector>

namespace TCP {

TcpClient::TcpClient(const char* addr, int port, TCP::logging_foo f_logger)
    : logger_(f_logger) {
  LClient logger(LClient::FConstructor, this, logger_);

  logger.Log("Creating sender socket", Debug);
  heartbeat_socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (heartbeat_socket_ < 0) {
    throw TcpException(TcpException::SocketCreation, logger_, errno);
  }
  sockaddr_in addr_conf = {.sin_family = AF_INET,
                           .sin_port = htons(port),
                           .sin_addr = {inet_addr(addr)}};
  logger.Log("Connecting heartbeat to server", Debug);
  if (connect(heartbeat_socket_, (sockaddr*)&addr_conf, sizeof(addr_conf)) <
      0) {
    close(heartbeat_socket_);
    throw TcpException(TcpException::Connection, logger_, errno);
  }
  logger.Log("Sending init mode to server", Debug);
  if (RawSend(heartbeat_socket_, "0", kULLMaxDigits + 1) != kULLMaxDigits + 1) {
    close(heartbeat_socket_);
    throw TcpException(TcpException::Sending, logger_, errno);
  }
  logger.Log("Waiting for password", Debug);
  if (!WaitForData(heartbeat_socket_, kLoopMsTimeout, logger, logger_)
           .has_value()) {
    logger.Log("Timeout", Error);
    close(heartbeat_socket_);
    throw TcpException(TcpException::Receiving, logger_);
  }
  logger.Log("Data is available. Trying to receive data", Debug);
  auto password_str = RawRecv(heartbeat_socket_, kULLMaxDigits + 1);
  if (password_str.size() != kULLMaxDigits + 1) {
    close(heartbeat_socket_);
    throw TcpException(TcpException::Receiving, logger_, errno);
  }

  if (std::stoll(password_str) == 0) {
    close(heartbeat_socket_);
    logger.Log("Got term signal", Warning);
    throw TcpException(TcpException::Acceptance, logger_);
  }
  logger.Log("Got password", Debug);

  logger.Log("Creating receiver socket", Debug);
  main_socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (main_socket_ < 0) {
    close(heartbeat_socket_);
    throw TcpException(TcpException::SocketCreation, logger_, errno);
  }
  logger.Log("Connecting main socket to server", Debug);
  if (connect(main_socket_, (sockaddr*)&addr_conf, sizeof(addr_conf)) < 0) {
    close(heartbeat_socket_);
    close(main_socket_);
    throw TcpException(TcpException::Connection, logger_, errno);
  }
  logger.Log("Sending password to server", Debug);
  if (RawSend(main_socket_, password_str, kULLMaxDigits + 1) !=
      kULLMaxDigits + 1) {
    close(heartbeat_socket_);
    close(main_socket_);
    throw TcpException(TcpException::Sending, logger_, errno);
  }

  logger.Log("Waiting for signal", Debug);
  if (!WaitForData(main_socket_, kLoopMsTimeout, logger, logger_).has_value()) {
    logger.Log("Timeout", Error);
    close(heartbeat_socket_);
    close(main_socket_);
    throw TcpException(TcpException::Receiving, logger_);
  }
  logger.Log("Data is available. Receiving", Debug);
  auto server_answ = RawRecv(main_socket_, 1);
  if (server_answ.size() != 1) {
    close(heartbeat_socket_);
    close(main_socket_);
    throw TcpException(TcpException::Receiving, logger_, errno);
  }
  if (std::stoi(server_answ) != 1) {
    logger.Log("Got term signal", Warning);
    close(heartbeat_socket_);
    close(main_socket_);
    throw TcpException(TcpException::Acceptance, logger_);
  }

  logger.Log("Creating threads", Debug);
  try {
    this_pointer_ = new TcpClient*(this);
    this_mutex_ = new std::mutex();
    heartbeat_thread_ =
        std::thread(&TcpClient::HeartBeat, this_pointer_, this_mutex_);
  } catch (std::exception& exception) {
    delete this_pointer_;
    delete this_mutex_;

    if (this_pointer_ == nullptr || this_mutex_ == nullptr) {
      logger.Log("Cannot allocate memory: " + std::string(exception.what()),
                 Error);

      throw exception;
    }
    logger.Log("Cannot create sender thread: " + std::string(exception.what()),
               Error);
    throw exception;
  }

  logger.Log("TcpClient created", Info);
}
TcpClient::TcpClient(TCP::TcpClient&& other) noexcept
    : main_socket_(other.main_socket_),
      heartbeat_socket_(other.main_socket_),
      heartbeat_thread_(std::move(other.heartbeat_thread_)),
      this_pointer_(other.this_pointer_),
      this_mutex_(other.this_mutex_),
      logger_(other.logger_) {
  if (!other.is_active_) {
    is_active_ = false;
    return;
  }
  LClient logger(LClient::FMoveConstructor, this, logger_);
  logger.Log("Building TCP-Client via move constructor", Debug);

  logger.Log("Locking mutex", Debug);
  this_mutex_->lock();
  logger.Log("Mutex locked", Debug);
  *this_pointer_ = this;
  this_mutex_->unlock();

  other.is_active_ = false;
  logger.Log("TCP-Client is built via move constructor", Info);
}
TcpClient::TcpClient(int heartbeat_socket, int main_socket,
                     logging_foo f_logger)
    : heartbeat_socket_(heartbeat_socket),
      main_socket_(main_socket),
      logger_(f_logger) {
  LClient logger(LClient::FFromServerConstructor, this, logger_);
  logger.Log("Building TCP-Client via move constructor", Debug);

  logger.Log("Sending heart beat", Debug);
  if (RawSend(heartbeat_socket_,
              std::to_string(
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count()),
              kULLMaxDigits + 1) != kULLMaxDigits + 1) {
    logger.Log("Error occurred while sending heart beat", Error);

    close(heartbeat_socket_);
    close(main_socket_);
    throw TcpException(TcpException::Sending, logger_, errno);
  }

  logger.Log("Heart beat sent. Creating heartbeat thread", Debug);
  try {
    this_pointer_ = new TcpClient*(this);
    this_mutex_ = new std::mutex();

    heartbeat_thread_ = std::thread(HeartBeat, this_pointer_, this_mutex_);
  } catch (std::exception& exception) {
    logger.Log("Error while creating thread", Error);
    close(heartbeat_socket_);
    close(main_socket_);
    delete this_pointer_;
    delete this_mutex_;

    throw exception;
  }
  logger.Log("TCP-Client is built via server constructor", Info);
}
TcpClient::~TcpClient() {
  StopClient();

  LClient(LClient::FDestructor, this, logger_)
      .Log("TCP-Client destructed", Info);
}

void TcpClient::StopClient() noexcept {
  LClient logger(LClient::FStopClient, this, logger_);

  if (!is_active_) {
    logger.Log("Client has already been stopped", Info);
    return;
  }
  logger.Log("Client is running. Setting term flag. Joining thread", Debug);
  is_active_ = false;
  heartbeat_thread_.join();
  logger.Log("Thread joined. Freeing resources", Debug);

  close(main_socket_);
  close(heartbeat_socket_);
  delete this_pointer_;
  delete this_mutex_;

  logger.Log("Client stopped", Info);
}

void TcpClient::HeartBeat(TCP::TcpClient** this_pointer,
                          std::mutex* this_mutex) noexcept {
  this_mutex->lock();
  logging_foo foo_log = (**this_pointer).logger_;
  LClient logger(LClient::FHeartBeatLoop, this_pointer, foo_log);

  int socket = (**this_pointer).heartbeat_socket_;

  int loop_timeout = (**this_pointer).kLoopMsTimeout;
  int no_answ_timeout = (**this_pointer).kNoAnswMsTimeout;

  this_mutex->unlock();

  logger.Log("Starting loop", Debug);

  auto last_connection = std::chrono::system_clock::now().time_since_epoch();
  while (true) {
    auto curr_time = std::chrono::system_clock::now();
    auto waiting = WaitForData(socket, loop_timeout * 2, logger, foo_log);

    this_mutex->lock();
    bool term_flag = (**this_pointer).is_active_;
    this_mutex->unlock();
    if (!term_flag) {
      return;
    }

    if (!waiting.has_value()) {
      if (std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch() -
              last_connection)
              .count() > no_answ_timeout) {
        this_mutex->lock();
        (**this_pointer).ms_ping_ = -1;
        this_mutex->unlock();
        return;
      }
      continue;
    }

    auto recv_beat = RawRecv(socket, kULLMaxDigits + 1);
    if (recv_beat.size() != kULLMaxDigits + 1) {
      this_mutex->lock();
      (**this_pointer).ms_ping_ = -1;
      this_mutex->unlock();

      TcpException(TcpException::Receiving, foo_log, errno);
      return;
    }
    auto sending_time = std::chrono::milliseconds(std::stoll(recv_beat));
    auto receiving_time = curr_time.time_since_epoch() +
                          std::chrono::milliseconds(waiting.value());

    int ping = std::chrono::duration_cast<std::chrono::milliseconds>(
                   receiving_time - sending_time)
                   .count();
    this_mutex->lock();
    (**this_pointer).ms_ping_ = ping > 0 ? ping : 1;
    this_mutex->unlock();

    std::this_thread::sleep_for(std::chrono::milliseconds(loop_timeout));
    auto send_curr_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    auto send_answ = RawSend(socket, std::to_string(send_curr_time.count()),
                             kULLMaxDigits + 1);
    if (send_answ != kULLMaxDigits + 1) {
      this_mutex->lock();
      (**this_pointer).ms_ping_ = -1;
      this_mutex->unlock();

      TcpException(TcpException::Sending, foo_log, errno);
      return;
    }

    last_connection = std::chrono::system_clock::now().time_since_epoch();
  }
}

std::string TcpClient::StrRecv(int ms_timeout, TCP::Logger& logger) {
  logger.Log("Starting waiting for data", Debug);
  if (!WaitForData(main_socket_, ms_timeout, logger, logger_).has_value()) {
    logger.Log("Timeout", Info);
    return "";
  }
  logger.Log("Data is available. Receiving", Debug);
  auto message_size = RawRecv(main_socket_, kULLMaxDigits + 1);
  if (message_size.empty()) {
    throw TcpException(TcpException::Receiving, logger_, errno);
  }
  if (message_size.size() != kULLMaxDigits + 1) {
    throw TcpException(TcpException::Receiving, logger_, 0, true);
  }

  size_t size = std::stoll(message_size);
  logger.Log("Message size: " + std::to_string(size), Debug);
  logger.Log("Checking for data availability", Debug);
  if (!WaitForData(main_socket_, 0, logger, logger_).has_value()) {
    logger.Log("Data is not available", Debug);
    throw TcpException(TcpException::Receiving, logger_, 0, true);
  }

  logger.Log("Main data is available. Receiving data", Debug);
  auto message = RawRecv(main_socket_, size);
  if (message.empty()) {
    throw TcpException(TcpException::Receiving, logger_, errno);
  }
  if (message.size() != size) {
    throw TcpException(TcpException::Receiving, logger_, 0, true);
  }
  logger.Log("Message received", Info);
  return message;
}
void TcpClient::StrSend(const std::string& message, TCP::Logger& logger) {
  logger.Log("Creating to_send string", Debug);
  std::string to_send = std::to_string(message.size());
  for (size_t i = to_send.size(); i < kULLMaxDigits + 1; ++i) {
    to_send.push_back('\0');
  }

  to_send += message;
  logger.Log("Trying to send data", Debug);
  auto answ = RawSend(main_socket_, to_send, to_send.size());
  if (answ < 0) {
    throw TcpException(TcpException::Sending, logger_, errno);
  }
  if (answ != to_send.size()) {
    throw TcpException(TcpException::Sending, logger_, 0, true);
  }
  logger.Log("Message sent successfully", Info);
}

bool TcpClient::IsAvailable() {
  LClient logger(LClient::FIsAvailable, this, logger_);
  logger.Log("Checking if peer connected", Debug);
  if (!IsConnected()) {
    logger.Log("Peer is not connected", Debug);
    throw TcpException(TcpException::ConnectionBreak, logger_);
  }

  logger.Log("Peer is connected. Checking data availability", Debug);
  return WaitForData(main_socket_, 0, logger, logger_).has_value();
}
bool TcpClient::IsConnected() noexcept {
  if (!is_active_) {
    return false;
  }
  if (ms_ping_ < 0) {
    StopClient();
    return false;
  }
  return true;
}

int TcpClient::GetPing() {
  if (ms_ping_ == -1) {
    throw TcpException(TcpException::ConnectionBreak, logger_);
  }
  return ms_ping_;
}

void TcpClient::ToArgs(std::stringstream& stream) {}
void TcpClient::FromArgs(std::string& output) {}

}  // namespace TCP