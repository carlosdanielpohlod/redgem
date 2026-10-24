#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "net/http/http_trust.hpp"
#include "net/socket/socket.hpp"

namespace redgem::net {

// What one try at moving bytes came to.
enum class Flow {
  Moved,      // some went (or a step was taken): try again at once
  WantRead,   // nothing until more arrives
  WantWrite,  // nothing until the connection takes more
  Ended,      // the other side closed it
  Cut,        // it ended without the other side saying so (TLS's close_notify): the end may be an attacker's
  Failed      // the connection broke, or the other side is not who it claims (see error())
};

// The bytes of one request's connection, as they are (http://) or through TLS (https://). Its
// socket never waits: Patience does the waiting.
class HttpStream {
 public:
  explicit HttpStream(Socket socket) : socket_(std::move(socket)) {}
  virtual ~HttpStream() = default;
  HttpStream(const HttpStream&) = delete;
  HttpStream& operator=(const HttpStream&) = delete;

  // Sends what goes now from the front of `bytes`, and takes it off.
  virtual Flow send(std::vector<std::uint8_t>& bytes) = 0;
  // Adds what arrived to `into`.
  virtual Flow receive(std::vector<std::uint8_t>& into) = 0;
  // Why it Failed.
  virtual std::string error() const = 0;
  const Socket& socket() const { return socket_; }

 protected:
  Socket socket_;
};

std::unique_ptr<HttpStream> plain_stream(Socket socket);
// TLS to `host`, whose certificate must be one `trust` vouches for; nullptr, with `error`, when
// TLS cannot even start.
std::unique_ptr<HttpStream> secure_stream(Socket socket, const std::string& host,
                                          std::shared_ptr<const TrustStore> trust, std::string& error);

// How long a request may still wait, on the thread it runs on: until its timeout, or until it is
// given up (its game ended).
class Patience {
 public:
  Patience(float seconds, const std::atomic<bool>& cancelled);
  // Sleeps until `socket` can go on (has something to read, or room to send when `sending`) or a
  // moment passed. False once there is no waiting left: why() says why.
  bool wait(const Socket& socket, bool sending) const;
  bool over() const;
  std::string why() const;

 private:
  std::chrono::steady_clock::time_point deadline_;
  float seconds_;
  const std::atomic<bool>& cancelled_;
};

}  // namespace redgem::net
