#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/clock.hpp"
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

// How a request is given up from another thread (its game ended): it is told so, and the
// connection it is waiting on, if it is, is hung up, so that the wait ends at once.
class HttpCancel {
 public:
  void cancel();
  bool cancelled() const { return cancelled_.load(); }

 private:
  friend class HeldConnection;
  mutable std::mutex mutex_;  // over held_, and over hanging it up: it is never hung up once let go
  std::atomic<bool> cancelled_{false};
  mutable const Socket* held_ = nullptr;
};

// The connection a request waits on while this lives: hung up at once when the request is given
// up, or already was. The socket must outlive it and stay where it is (not moved, not closed).
class HeldConnection {
 public:
  HeldConnection(const HttpCancel& cancel, const Socket& socket);
  ~HeldConnection();
  HeldConnection(const HeldConnection&) = delete;
  HeldConnection& operator=(const HeldConnection&) = delete;

 private:
  const HttpCancel& cancel_;
};

// How long a request may still wait, on the thread it runs on: until its timeout, on `clock`, or
// until it is given up (its game ended).
class Patience {
 public:
  Patience(float seconds, const HttpCancel& cancel, const Clock& clock);
  // Sleeps until `socket` can go on (has something to read, or room to send when `sending`) or a
  // moment passed. False once there is no waiting left: why() says why.
  bool wait(const Socket& socket, bool sending) const;
  bool over() const;
  std::string why() const;

 private:
  const Clock& clock_;
  double deadline_;
  float seconds_;
  const HttpCancel& cancel_;
};

}  // namespace redgem::net
