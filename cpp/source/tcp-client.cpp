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

TcpClient::TcpClient(const char* addr, int port, int ms_ping_threshold,
                     int ms_loop_period, logging_foo f_logger) {
  Connect(addr, port, ms_ping_threshold, ms_loop_period, f_logger);
}

TcpClient::TcpClient(const char* addr, int port, int ms_ping_threshold,
                     logging_foo f_logger)
    : TcpClient(addr, port, ms_ping_threshold, kDefLoopPeriod, f_logger) {}
TcpClient::TcpClient(const char* addr, int port, TCP::logging_foo f_logger)
    : TcpClient(addr, port, kDefPingThreshold, kDefLoopPeriod, f_logger) {}

TcpClient::TcpClient(TCP::TcpClient&& other) noexcept
    : main_socket_(other.main_socket_),
      heartbeat_socket_(other.heartbeat_socket_),
      ping_threshold_(other.ping_threshold_),
      loop_period_(other.loop_period_),
      heartbeat_thread_(std::move(other.heartbeat_thread_)),
      this_pointer_(other.this_pointer_),
      this_mutex_(other.this_mutex_),
      is_active_(other.is_active_),
      ms_ping_(other.ms_ping_),
      logger_(other.logger_) {
  if (!is_active_) {
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
TcpClient::TcpClient(int heartbeat_socket, int main_socket, int ping_threshold,
                     int loop_period, logging_foo f_logger)
    : heartbeat_socket_(heartbeat_socket),
      main_socket_(main_socket),
      ping_threshold_(ping_threshold),
      loop_period_(loop_period),
      logger_(f_logger) {
  LClient logger(LClient::FFromServerConstructor, this, logger_);
  logger.Log("Building TCP-Client via move constructor", Debug);

  is_active_ = true;

  logger.Log("Creating heartbeat thread", Debug);
  try {
    this_pointer_ = new TcpClient*(this);
    this_mutex_ = new std::mutex();

    heartbeat_thread_ =
        std::thread(HeartBeatServer, this_pointer_, this_mutex_);
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

TcpClient& TcpClient::operator=(TCP::TcpClient&& other) {
  LClient logger(LClient::FMoveAssignmentOperator, this, logger_);
  logger.Log("Method run from " + GetAddress(&other), Info);
  logger.Log("Stopping client", Debug);

  StopClient();
  main_socket_ = other.main_socket_;
  heartbeat_socket_ = other.heartbeat_socket_;
  ping_threshold_ = other.ping_threshold_;
  loop_period_ = other.loop_period_;
  heartbeat_thread_ = std::move(other.heartbeat_thread_);
  this_pointer_ = other.this_pointer_;
  this_mutex_ = other.this_mutex_;
  is_active_ = other.is_active_;
  ms_ping_ = other.ms_ping_;
  logger_ = other.logger_;

  logger = LClient(LClient::FMoveAssignmentOperator, this, logger_);

  logger.Log("Variables copied", Info);

  if (!is_active_){
    logger.Log("Client is not active. Disconnecting", Info);
    return *this;
  }

  logger.Log("Changing this_pointer. Locking mutex", Debug);
  this_mutex_->lock();
  *this_pointer_ = this;
  this_mutex_ -> unlock();

  other.is_active_ = false;

  logger.Log("Client assigned successfully", Info);
  return *this;
}

void TcpClient::Connect(const char* addr, int port, int ms_ping_threshold,
                        int ms_loop_period, logging_foo f_logger) {
  if (is_active_) {
    throw TcpException(TcpException::Connection, f_logger);
  }

  ping_threshold_ = ms_ping_threshold;
  loop_period_ = ms_loop_period;
  logger_ = f_logger;

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
  if (!WaitForData(heartbeat_socket_, ping_threshold_, logger, logger_)
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

  if (main_socket_ < 0 || !SetKeepIdle(main_socket_)) {
    close(heartbeat_socket_);
    if (main_socket_ >= 0) {
      close(main_socket_);
    }
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
  if (!WaitForData(main_socket_, ping_threshold_, logger, logger_)
           .has_value()) {
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
    is_active_ = true;
    heartbeat_thread_ =
        std::thread(&TcpClient::HeartBeatClient, this_pointer_, this_mutex_);
  } catch (std::exception& exception) {
    is_active_ = false;
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
void TcpClient::Connect(const char* addr, int port, int ms_ping_threshold,
                        logging_foo f_logger) {
  Connect(addr, port, ms_ping_threshold, kDefLoopPeriod, f_logger);
}
void TcpClient::Connect(const char* addr, int port, logging_foo f_logger) {
  Connect(addr, port, kDefPingThreshold, kDefLoopPeriod, f_logger);
}

std::string TcpClient::RecvStr(int ms_timeout) {
  LClient logger(LClient::FRecv, this, logger_);
  logger.Log("Starting receiving method", Debug);

  if (!is_active_) {
    CheckReceiveError();
  }

  logger.Log("Receiving string", Debug);
  auto recv_str = StrRecv(ms_timeout, logger);
  logger.Log(
      "Method returned string of size " + std::to_string(recv_str.size()),
      Debug);

  return recv_str;
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

void TcpClient::HeartBeatClient(TCP::TcpClient** this_pointer,
                                std::mutex* this_mutex) noexcept {
  this_mutex->lock();
  logging_foo foo_log = (**this_pointer).logger_;
  LClient logger(LClient::FHeartBeatLoop, this_pointer, foo_log);

  int socket = (**this_pointer).heartbeat_socket_;
  int ping_threshold = (**this_pointer).ping_threshold_;
  int loop_period = (**this_pointer).loop_period_;

  this_mutex->unlock();

  logger.Log("Starting loop", Debug);

  try {
    auto last_connection = std::chrono::system_clock::now().time_since_epoch();
    while (true) {
      logger.Log("Starting waiting for ping", Debug);
      auto curr_time = std::chrono::system_clock::now().time_since_epoch();
      auto waiting = WaitForData(socket, loop_period, logger, foo_log);
      logger.Log("Checking term flag", Debug);

      this_mutex->lock();
      bool term_flag = (**this_pointer).is_active_;
      this_mutex->unlock();
      if (!term_flag) {
        return;
      }

      if (!waiting.has_value()) {
        logger.Log("Ping is not available", Debug);
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch() -
                last_connection)
                .count() > ping_threshold) {
          logger.Log("Connection timeout. Disconnecting", Info);
          this_mutex->lock();
          (**this_pointer).ms_ping_ = -1;
          this_mutex->unlock();
          return;
        }
        logger.Log("Connection is not timeout", Debug);
        continue;
      }

      logger.Log("Ping is available. Receiving", Debug);
      auto ping_str = RawRecv(socket, kULLMaxDigits + 1);
      if (ping_str.size() != kULLMaxDigits + 1) {
        logger.Log("Error occurred while receiving", Warning);
        this_mutex->lock();
        (**this_pointer).ms_ping_ = -1;
        this_mutex->unlock();

        TcpException(TcpException::Receiving, foo_log, errno);
        return;
      }
      logger.Log("Ping received. Setting", Debug);
      this_mutex->lock();
      (**this_pointer).ms_ping_ = std::stoi(ping_str);
      this_mutex->unlock();

      logger.Log("Sending delay", Debug);
      auto recv_time = curr_time + std::chrono::milliseconds(waiting.value());
      auto send_recv_diff =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch() - recv_time);
      if (RawSend(socket, std::to_string(send_recv_diff.count()),
                  kULLMaxDigits + 1) != kULLMaxDigits + 1) {
        logger.Log("Error occurred while sending", Warning);
        this_mutex->lock();
        (**this_pointer).ms_ping_ = -1;
        this_mutex->unlock();

        TcpException(TcpException::Sending, foo_log, errno);
        return;
      }
      last_connection = std::chrono::system_clock::now().time_since_epoch();
      logger.Log("Sending is successful", Debug);
    }
  } catch (TcpException& exception) {
    logger.Log("Exception caught: " + std::string(exception.what()), Warning);
    this_mutex->lock();
    (**this_pointer).ms_ping_ = -1;
    this_mutex->unlock();
    return;
  }
}

void TcpClient::HeartBeatServer(TCP::TcpClient** this_pointer,
                                std::mutex* this_mutex) noexcept {
  this_mutex->lock();
  logging_foo foo_log = (**this_pointer).logger_;
  LClient logger(LClient::FHeartBeatLoop, this_pointer, foo_log);

  int socket = (**this_pointer).heartbeat_socket_;
  int ping_threshold = (**this_pointer).ping_threshold_;
  int loop_period = (**this_pointer).loop_period_;

  this_mutex->unlock();

  logger.Log("Starting loop", Debug);

  try {
    while (true) {
      logger.Log("Getting flag and cached ping", Debug);
      this_mutex->lock();
      bool term_flag = (**this_pointer).is_active_;
      int cached_ping = (**this_pointer).ms_ping_;
      this_mutex->unlock();
      if (!term_flag) {
        return;
      }
      logger.Log("Variables got. Sending ping", Debug);

      auto send_time = std::chrono::system_clock::now().time_since_epoch();
      if (RawSend(socket, std::to_string(cached_ping), kULLMaxDigits + 1) !=
          kULLMaxDigits + 1) {
        logger.Log("Error occurred while sending ping", Warning);
        this_mutex->lock();
        (**this_pointer).ms_ping_ = -1;
        this_mutex->unlock();
        return;
      }

      logger.Log("Waiting for delay", Debug);

      auto waiting =
          WaitForData(socket, loop_period + ping_threshold, logger, foo_log);
      auto recv_time = std::chrono::system_clock::now().time_since_epoch();

      if (!waiting.has_value()) {
        logger.Log("Waiting timeout. Terminating", Warning);
        this_mutex->lock();
        (**this_pointer).ms_ping_ = -1;
        this_mutex->unlock();
        return;
      }

      logger.Log("Data is available. Receiving", Debug);
      auto delay_str = RawRecv(socket, kULLMaxDigits + 1);
      if (delay_str.size() != kULLMaxDigits + 1) {
        logger.Log("Error occurred while receiving", Warning);
        this_mutex->lock();
        (**this_pointer).ms_ping_ = -1;
        this_mutex->unlock();

        TcpException(TcpException::Receiving, foo_log, errno);
        return;
      }

      logger.Log("Computing ping", Debug);
      auto curr_ping = std::chrono::duration_cast<std::chrono::milliseconds>(
          recv_time - send_time -
          std::chrono::milliseconds(std::stoll(delay_str)));
      curr_ping /= 2;

      logger.Log("Setting ping: " + std::to_string(curr_ping.count()), Debug);
      this_mutex->lock();
      (**this_pointer).ms_ping_ = curr_ping.count();
      this_mutex->unlock();

      logger.Log("Continuing", Debug);
      std::this_thread::sleep_for(std::chrono::milliseconds(loop_period));
    }
  } catch (TcpException& exception) {
    logger.Log("Exception caught: " + std::string(exception.what()), Warning);
    this_mutex->lock();
    (**this_pointer).ms_ping_ = -1;
    this_mutex->unlock();
    return;
  }
}

std::string TcpClient::StrRecv(int ms_timeout, TCP::Logger& logger) {
  logger.Log("Starting waiting for data", Debug);
  if (!WaitForData(main_socket_, ms_timeout, logger, logger_).has_value()) {
    logger.Log("Timeout. Checking is peer is connected", Info);
    CheckReceiveError();
    logger.Log("Peer is connected", Info);
    return "";
  }
  logger.Log("Data is available. Receiving", Debug);
  auto control_block = RawRecv(main_socket_, (kULLMaxDigits + 1) * 2);
  if (control_block.empty()) {
    CheckReceiveError();
    throw TcpException(TcpException::Receiving, logger_, errno);
  }
  if (control_block.size() != (kULLMaxDigits + 1) * 2) {
    throw TcpException(TcpException::Receiving, logger_, 0, true);
  }

  auto delimiter = control_block.begin() + control_block.find(' ') + 1;
  size_t full_block_num =
      std::stoll(std::string(control_block.begin(), delimiter));
  size_t last_block_size =
      std::stoll(std::string(delimiter, control_block.end()));

  logger.Log("Number of full blocks: " + std::to_string(full_block_num) +
                 ". Last block size: " + std::to_string(last_block_size),
             Debug);

  std::string result;

  logger.Log("Receiving main data", Debug);
  for (size_t i = 0; i < full_block_num; ++i) {
    auto block = RawRecv(main_socket_, BLOCK_SIZE);
    if (block.empty()) {
      throw TcpException(TcpException::Receiving, logger_, errno);
    }
    result += block;
  }
  auto part_block = RawRecv(main_socket_, last_block_size + 1);
  if (part_block.empty()) {
    throw TcpException(TcpException::Receiving, logger_, errno);
  }

  result += part_block;
  logger.Log("Message received", Info);
  return result;
}
void TcpClient::StrSend(const std::string& message, TCP::Logger& logger) {
  logger.Log("Creating to_send string", Debug);
  size_t full_block_num = message.size() / BLOCK_SIZE;
  size_t last_block_size = message.size() - (full_block_num * BLOCK_SIZE);

  logger.Log("Trying to send data", Debug);
  auto ctrl_answ = RawSend(
      main_socket_,
      std::to_string(full_block_num) + " " + std::to_string(last_block_size),
      (kULLMaxDigits + 1) * 2);
  if (ctrl_answ < 0) {
    throw TcpException(TcpException::Sending, logger_, errno);
  }
  if (ctrl_answ != (kULLMaxDigits + 1) * 2) {
    throw TcpException(TcpException::Sending, logger_, 0, true);
  }

  logger.Log("Sending main block", Debug);
  for (size_t i = 0; i < full_block_num; ++i) {
    auto answ =
        RawSend(main_socket_,
                std::string(message.c_str() + (i * BLOCK_SIZE), BLOCK_SIZE),
                BLOCK_SIZE);
    if (answ < 0) {
      throw TcpException(TcpException::Sending, logger_, errno);
    }
    if (answ != BLOCK_SIZE) {
      throw TcpException(TcpException::Sending, logger_, 0, true);
    }
  }
  auto answ =
      RawSend(main_socket_,
              std::string(message.c_str() + (full_block_num * BLOCK_SIZE),
                          last_block_size),
              last_block_size + 1);
  if (answ < 0) {
    throw TcpException(TcpException::Sending, logger_, errno);
  }
  if (answ != last_block_size + 1) {
    throw TcpException(TcpException::Sending, logger_, 0, true);
  }

  logger.Log("Message sent successfully", Info);
}

bool TcpClient::IsAvailable() {
  LClient logger(LClient::FIsAvailable, this, logger_);
  logger.Log("Checking data availability", Debug);

  if (!is_active_) {
    CheckReceiveError();
  }

  bool availability = WaitForData(main_socket_, 0, logger, logger_).has_value();
  if (availability) {
    return true;
  }
  logger.Log("Checking if peer connected", Debug);
  if (!IsConnected()) {
    logger.Log("Peer is not connected. Stopping client", Debug);
    StopClient();
    throw TcpException(TcpException::ConnectionBreak, logger_);
  }

  logger.Log("Peer is connected", Debug);
  return false;
}
bool TcpClient::IsConnected() noexcept {
  if (!is_active_) {
    return false;
  }
  return ms_ping_ >= 0;
}

int TcpClient::GetPing() {
  if (ms_ping_ == -1) {
    throw TcpException(TcpException::ConnectionBreak, logger_);
  }
  return ms_ping_;
}

void TcpClient::ToArgs(std::stringstream& stream) {}
void TcpClient::FromArgs(std::string& output) {}

void TcpClient::CheckReceiveError() {
  if (!IsConnected()) {
    StopClient();
    throw TcpException(TcpException::ConnectionBreak, logger_, errno);
  }
}

int TcpClient::GetMsPingThreshold() const noexcept { return ping_threshold_; }

}  // namespace TCP