#include "net/http/http_client.hpp"

#include <atomic>
#include <optional>
#include <system_error>
#include <thread>

#include "core/engine_thread.hpp"
#include "net/http/http_exchange.hpp"

namespace redgem::net {

struct HttpClient::Job {
  int id = 0;
  std::atomic<bool> cancelled{false};
  std::atomic<bool> done{false};
  HttpResponse response;  // written by the thread before `done`, read by poll after it

  // Carries the request out on a thread of its own, which holds the job (throws std::system_error
  // when the system has no thread left).
  static void start(const std::shared_ptr<Job>& job, HttpRequest request, Url url, const HttpClient& client) {
    std::thread([job, request = std::move(request), url = std::move(url), trust = client.trust_,
                 clock = client.clock_] {
      run_engine_thread([&] { job->carry_out(request, url, trust, *clock); });
    }).detach();
  }

  void carry_out(const HttpRequest& request, const Url& url, const std::shared_ptr<const TrustStore>& trust,
                 const Clock& clock) {
    response = exchange(request, url, trust, cancelled, clock);
    done.store(true);
  }
};

namespace {

// The program's steady clock, shared by nobody: it lasts as long as the program.
std::shared_ptr<const Clock> shared_steady_clock() { return {std::shared_ptr<const Clock>(), &steady_clock()}; }

}  // namespace

HttpClient::HttpClient(std::shared_ptr<const TrustStore> trust, std::shared_ptr<const Clock> clock)
    : trust_(std::move(trust)), clock_(clock ? std::move(clock) : shared_steady_clock()) {}

HttpClient::~HttpClient() {
  // The threads hold their jobs themselves: they end on their own, with nobody to tell.
  for (const auto& job : jobs_) job->cancelled.store(true);
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
    } else {
      still_out.push_back(std::move(job));
    }
  }
  jobs_.swap(still_out);
  return results;
}

}  // namespace redgem::net
