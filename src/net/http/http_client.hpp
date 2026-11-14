#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "core/clock.hpp"
#include "net/http/http_request.hpp"
#include "net/http/http_trust.hpp"
#include "net/http/http_url.hpp"

namespace redgem::net {

// The answer to one request, under the number start() gave it.
struct HttpResult {
  int id = 0;
  HttpResponse response;
};

// How long a client that goes waits for the threads of its requests still on their way, once it
// gave them up: they end at once, unless one is busy with what cannot be cut short (looking its
// server's name up, which the system cannot be made to stop, most likely), and such a thread is
// left behind. The thread that ends the client (a game's, an editor's, a server room's) waits
// that long only then.
inline constexpr std::chrono::milliseconds kHttpShutdownWait{2000};

// The web requests of one game (HTTP in Ruby). Each runs on a thread of its own, so the game never
// waits; poll() collects the answers on the game's thread, as the network's events are. When the
// client goes (its game ended), the requests still on their way are given up, their connections
// hung up, and their threads joined, kHttpShutdownWait at most: nobody hears of them.
class HttpClient {
 public:
  // Secure answers are checked against `trust`; the system's authorities when null. Timeouts are
  // kept on `clock`, the steady clock when null (shared: a request's thread left behind may outlive
  // the client).
  explicit HttpClient(std::shared_ptr<const TrustStore> trust = nullptr, std::shared_ptr<const Clock> clock = nullptr);
  ~HttpClient();
  HttpClient(const HttpClient&) = delete;
  HttpClient& operator=(const HttpClient&) = delete;

  // Sends a request: the number its answer will come under. 0, with `error`, when it cannot go as
  // it is (an address or a header that is no good). With kMaxHttpRequestsInFlight already on their
  // way, its answer is an error, at the next poll.
  int start(HttpRequest request, std::string& error);
  // The answers that came since the last call.
  std::vector<HttpResult> poll();
  // Requests sent whose answers were not collected yet.
  std::size_t in_flight() const { return jobs_.size(); }

 private:
  // A request's own, shared with the thread that carries it out.
  struct Job;

  void launch(int id, HttpRequest request, Url url);

  std::shared_ptr<const TrustStore> trust_;
  std::shared_ptr<const Clock> clock_;
  std::vector<std::shared_ptr<Job>> jobs_;
  std::vector<HttpResult> refused_;  // answered at once: for the next poll
  int next_id_ = 1;
};

}  // namespace redgem::net
