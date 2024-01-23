#include "tcp-server.hpp"

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <list>
#include <string>

#include "tcp-supply.hpp"

namespace TCP {

TcpServer::TcpServer(int port, int ms_ping_threshold, int ms_loop_period,
                     logging_foo f_logger)
    : port_(port),
      ping_threshold_(ms_ping_threshold),
      loop_period_(ms_loop_period),
      logger_(f_logger) {
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

  logger.Log("Trying to bind to " + std::to_string(port), Debug);
  if (bind(listener_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    close(listener_);
    throw TcpException(TcpException::Binding, logger_, errno);
  }
  logger.Log("Bound", Debug);

  logger.Log("Trying to listen", Debug);
  if (listen(listener_, kMaxClientLength) < 0) {
    close(listener_);
    throw TcpException(TcpException::Listening, logger_, errno);
  }
  logger.Log("Listening", Debug);

  logger.Log("Creating accepter thread", Debug);
  accept_thread_ = std::thread(&TcpServer::AcceptLoop, this);
  logger.Log(
      "Server on port " + std::to_string(port) + " successfully launcher",
      Info);
}

TcpServer::TcpServer(int port, int ms_ping_threshold, TCP::logging_foo f_logger)
    : TcpServer(port, ms_ping_threshold, kDefLoopPeriod, f_logger) {}
TcpServer::TcpServer(int port, TCP::logging_foo f_logger)
    : TcpServer(port, kDefPingThreshold, kDefLoopPeriod, f_logger) {}

TcpServer::~TcpServer() {
  LServer logger(LServer::FDestructor, this, logger_);

  CloseListener();

  logger.Log("Server destructed", Info);
}

TcpClient TcpServer::AcceptConnection() {
  LServer logger(LServer::FAccepter, this, logger_);

  logger.Log("Waiting for data is available", Debug);
  accepter_semaphore_.acquire();
  logger.Log("Get data availability flag. Checking if active", Debug);

  if (!is_active_) {
    logger.Log("Server is not active", Info);
    accepter_semaphore_.release();
    throw TcpException(TcpException::ConnectionBreak, logger_);
  }

  logger.Log("Active. Locking mutex", Debug);
  accept_mutex_.lock();
  logger.Log("Mutex locked", Debug);

  if (accepted_.empty()) {
    logger.Log("Data is not available", Warning);
    accept_mutex_.unlock();
    throw TcpException(TcpException::NoData, logger_);
  }

  logger.Log("Extracting client", Debug);
  TcpClient client = std::move(accepted_.front());
  accepted_.pop();

  logger.Log("Unlocking mutex", Debug);
  accept_mutex_.unlock();

  logger.Log("Returning client", Debug);
  return client;
}

void TcpServer::CloseListener() noexcept {
  LServer logger(LServer::FCloseListener, this, logger_);

  if (is_active_) {
    is_active_ = false;
    close(listener_);
    logger.Log("Listener closed", Debug);
    listener_ = 0;
    logger.Log("Waiting for accepter joining", Debug);
    accept_thread_.join();
    logger.Log("Listener closed. Accepter joined", Info);

    logger.Log("Releasing accepter waiters", Debug);
    accepter_semaphore_.release();
  } else {
    logger.Log("Listener is already closed", Info);
  }
}
bool TcpServer::IsListenerOpen() const noexcept { return is_active_; }

void TcpServer::AcceptLoop() noexcept {
  LServer logger(LServer::FLoopAccepter, this, logger_);
  logger.Log("Starting accepter loop", Debug);

  int64_t password = 1;
  while (is_active_) {
    try {
      logger.Log("Starting waiting for acceptance", Debug);
      if (!WaitForData(listener_, loop_period_, logger, logger_).has_value()) {
        logger.Log("Acceptance timeout", Debug);
        continue;
      }
      logger.Log("Client is waiting for accept. Accepting", Debug);
      int client = accept(listener_, NULL, NULL);

      if (!is_active_) {
        logger.Log("Got terminating flag. Closing connection. Terminating",
                   Debug);
        close(client);
        return;
      }
      if (client < 0) {
        logger.Log("Error occurred while accepting connection", Warning);
        TcpException(TcpException::Acceptance, logger_, errno);
        continue;
      }
      logger.Log("Connection accepted", Debug);

      logger.Log("Waiting for client to send password", Debug);
      try {
        if (!WaitForData(client, ping_threshold_, logger, logger_)
                 .has_value()) {
          logger.Log("Waiting timeout. Sending term signal", Warning);
          RawSend(client, "0", kULLMaxDigits + 1);
          continue;
        }
      } catch (...) {
        close(client);
        throw;
      }
      logger.Log("Client config is available. Trying to receive", Debug);
      auto mode_str = RawRecv(client, kULLMaxDigits + 1);
      if (mode_str.empty()) {
        logger.Log("Error occurred while receiving config. Sending term signal",
                   Warning);
        TcpException(TcpException::Receiving, logger_, errno);
        RawSend(client, "0", kULLMaxDigits + 1);
        close(client);
        continue;
      }

      int64_t client_password = std::stoll(mode_str);
      logger.Log("Got client config", Debug);

      if (client_password == 0) {
        logger.Log("Client is in init mode. Sending password", Debug);
        if (RawSend(client, std::to_string(password), kULLMaxDigits + 1) ==
            kULLMaxDigits + 1) {
          logger.Log("Password sent successfully", Debug);
          uncomplete_client_.erase(password);
          uncomplete_client_.insert({password, client});
        } else {
          logger.Log(
              "Error occurred while sending password. Closing connection",
              Warning);
          close(client);
        }
      } else if (uncomplete_client_.contains(client_password)) {
        logger.Log("Client sent password. Tmp table contains connected peer",
                   Debug);
        int client_recv = uncomplete_client_[client_password];
        uncomplete_client_.erase(client_password);
        if (RawSend(client, "1", 1) == 1) {
          logger.Log("Sent run signal. Locking mutex", Debug);
          accept_mutex_.lock();
          logger.Log("Mutex locked. Creating TcpClient", Debug);
          accepted_.emplace(TcpClient(client_recv, client, ping_threshold_,
                                      loop_period_, logger_));
          accept_mutex_.unlock();
          accepter_semaphore_.release();
          logger.Log("Mutex unlocked", Debug);
        } else {
          logger.Log(
              "Error occurred while sending run signal. Closing connections",
              Warning);
          close(client);
          close(client_recv);
        }
      } else {
        logger.Log(
            "Client sent password. Tmp table does not contain connected peer",
            Warning);
        RawSend(client, "0", 1);
        close(client);
      }
    } catch (TcpException& exception) {
      logger.Log("Caught exception", Debug);
    }
    password = (password % LONG_LONG_MAX) + 1;
  }
}

}  // namespace TCP