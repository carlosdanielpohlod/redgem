#include "net/http/http_client.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <system_error>
#include <thread>

#include "core/engine_thread.hpp"
#include "core/log.hpp"
#include "net/http/http_exchange.hpp"
#include "net/http/http_stream.hpp"

namespace redgem::net {

struct HttpClient::Job {
  using Deadline = std::chrono::steady_clock::time_point;

  int id = 0;
  HttpCancel cancel;
  std::atomic<bool> done{false};
  HttpResponse response;  // written by the thread before `done`, read by poll after it
  std::thread thread;     // joined by the client (or, stuck past kHttpShutdownWait, left behind)

  // Carries the request out on a thread of its own, which holds the job too (throws
  // std::system_error when the system has no thread left).
  static void start(const std::shared_ptr<Job>& job, HttpRequest request, Url url, const HttpClient& client) {
    job->thread = std::thread([job, request = std::move(request), url = std::move(url), trust = client.trust_,
                               clock = client.clock_] {
      run_engine_thread([&] { job->carry_out(request, url, trust, *clock); });
      job->finish();
    });
  }

  void carry_out(const HttpRequest& request, const Url& url, const std::shared_ptr<const TrustStore>& trust,
                 const Clock& clock) {
    response = exchange(request, url, trust, cancel, clock);
    done.store(true);
  }

  // The thread's last word: its work is over.
  void finish() {
    const std::lock_guard<std::mutex> lock(mutex_);
    finished_ = true;
    ended_.notify_all();
  }

  // Joins the thread once it ended, by `deadline` at most: false when it had not, and is left behind.
  bool end_by(Deadline deadline) {
    std::unique_lock<std::mutex> lock(mutex_);
    const bool ended = ended_.wait_until(lock, deadline, [this] { return finished_; });
    lock.unlock();
    if (ended) {
      thread.join();
    } else {
      thread.detach();
    }
    return ended;
  }

 private:
  std::mutex mutex_;
  std::condition_variable ended_;
  bool finished_ = false;
};

namespace {

// The program's steady clock, shared by nobody: it lasts as long as the program.
std::shared_ptr<const Clock> shared_steady_clock() { return {std::shared_ptr<const Clock>(), &steady_clock()}; }

}  // namespace

HttpClient::HttpClient(std::shared_ptr<const TrustStore> trust, std::shared_ptr<const Clock> clock)
    : trust_(std::move(trust)), clock_(clock ? std::move(clock) : shared_steady_clock()) {}

HttpClient::~HttpClient() {
  // Every request is given up first: their threads end together, not one after the other.
  for (const auto& job : jobs_) job->cancel.cancel();
  const auto deadline = std::chrono::steady_clock::now() + kHttpShutdownWait;
  std::size_t left_behind = 0;
  for (const auto& job : jobs_) {
    if (!job->end_by(deadline)) ++left_behind;
  }
  if (left_behind > 0) {
    log::warning(std::to_string(left_behind) + " web request(s) still looking up a server's name when the game ended " +
                 "were left behind");
  }
}

int HttpClient::start(HttpRequest request, std::string& error) {
  error = request_problem(request);
  std::optional<Url> url = error.empty() ? parse_url(request.url, error) : std::nullopt;
  if (!url) return 0;
  const int id = next_id_++;
  if (jobs_.size() < kMaxHttpRequestsInFlight) {
    launch(id, std::move(request), std::move(*url));
  } else {
    refused_.push_back({id, HttpResponse::failure("too many requests on their way at once (at most " +
                                                  std::to_string(kMaxHttpRequestsInFlight) + ")")});
  }
  return id;
}

void HttpClient::launch(int id, HttpRequest request, Url url) {
  auto job = std::make_shared<Job>();
  job->id = id;
  try {
    Job::start(job, std::move(request), std::move(url), *this);
    jobs_.push_back(std::move(job));
  } catch (const std::system_error&) {
    refused_.push_back({id, HttpResponse::failure("the system has no thread left to send the request on")});
  }
}

std::vector<HttpResult> HttpClient::poll() {
  std::vector<HttpResult> results;
  results.swap(refused_);
  // (Each job is looked at once: one that ends meanwhile is collected at the next poll.)
  std::vector<std::shared_ptr<Job>> still_out;
  for (auto& job : jobs_) {
    if (job->done.load()) {
      results.push_back({job->id, std::move(job->response)});
      job->thread.join();  // (its work is over: it is ending)
    } else {
      still_out.push_back(std::move(job));
    }
  }
  jobs_.swap(still_out);
  return results;
}

}  // namespace redgem::net
