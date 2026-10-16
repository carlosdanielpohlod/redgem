#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "net/socket/socket.hpp"

namespace redgem::test {

// A web server on this machine for the tests of HTTP, on a thread of its own: plain, or TLS with a
// certificate made for the test, which nothing trusts but the clients the test hands it to
// (certificate_pem). It answers every request with the same bytes, or keeps quiet. Nothing it
// does reaches beyond this machine.
class HttpTestServer {
 public:
  enum class Kind { Plain, Secure };

  HttpTestServer(Kind kind, std::string answer);
  ~HttpTestServer();
  HttpTestServer(const HttpTestServer&) = delete;
  HttpTestServer& operator=(const HttpTestServer&) = delete;

  // Set before any request: it reads requests and never answers them.
  void keep_quiet() { quiet_ = true; }
  // https://localhost:<port><path>, or http://127.0.0.1:<port><path>.
  std::string url(const std::string& path) const;
  const std::string& certificate_pem() const { return certificate_pem_; }
  // The last request it read whole, as its bytes arrived.
  std::string last_request() const;
  int requests() const { return requests_.load(); }
  // Whether a client hung up while it was being kept waiting (it gave its request up).
  bool saw_hang_up() const { return hang_ups_.load() > 0; }

 private:
  struct Tls;  // the server's certificate and key

  void serve();
  void serve_one(net::Socket socket);
  bool stopping() const { return stop_.load(); }

  Kind kind_;
  std::string answer_;
  std::string certificate_pem_;
  std::unique_ptr<Tls> tls_;
  net::Socket listener_;
  std::uint16_t port_ = 0;
  std::atomic<bool> quiet_{false};
  std::atomic<bool> stop_{false};
  std::atomic<int> requests_{0};
  std::atomic<int> hang_ups_{0};
  mutable std::mutex mutex_;
  std::string last_request_;
  std::thread thread_;
};

// An answer with that status line and body (and its length).
std::string http_answer(const std::string& status, const std::string& body,
                        const std::string& extra_headers = "Content-Type: application/json\r\n");

}  // namespace redgem::test
