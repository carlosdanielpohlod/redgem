#include "editor/build_job.hpp"

#include <cstdio>
#include <exception>
#include <utility>

#include "core/engine_thread.hpp"

namespace redgem {
namespace {

constexpr int kSecondsPerMinute = 60;
constexpr int kSecondsPerHour = 60 * kSecondsPerMinute;
constexpr int kElapsedTextSize = 32;  // room for "hhhh:mm:ss" and more

}  // namespace

BuildJob::~BuildJob() { wait(); }

bool BuildJob::start(BuildRequest request) {
  if (busy_) return false;
  busy_ = true;
  target_ = request.choice.name;
  started_ = Clock::now();
  step_.clear();
  worker_ = std::thread([this, request = std::move(request)] { run_engine_thread([&] { work(request); }); });
  return true;
}

std::string BuildJob::step() const {
  const std::lock_guard<std::mutex> hold(lock_);
  return step_;
}

double BuildJob::seconds() const { return std::chrono::duration<double>(Clock::now() - started_).count(); }

std::optional<BuildOutcome> BuildJob::take_outcome() {
  if (!ended()) return std::nullopt;
  wait();  // (its thread is past the outcome: this is at most a moment)
  busy_ = false;
  return std::exchange(outcome_, std::nullopt);
}

void BuildJob::wait() {
  if (worker_.joinable()) worker_.join();
}

bool BuildJob::ended() const {
  const std::lock_guard<std::mutex> hold(lock_);
  return outcome_.has_value();
}

void BuildJob::work(const BuildRequest& request) {
  BuildOutcome outcome;
  outcome.target = request.choice.name;
  outcome.out_dir = request.out_dir;
  {
    // (The console is the editor's thread's: what the build logs waits for it in the outcome.)
    const log::Capture capture(
        [&outcome](log::Level level, const std::string& text) { outcome.messages.push_back({level, text}); });
    outcome.made = made_by(request, outcome.error);
  }
  outcome.seconds = seconds();
  const std::lock_guard<std::mutex> hold(lock_);
  outcome_ = std::move(outcome);
}

// What the build made. A build that throws failed, with what it threw as why: an exception never
// leaves the build's thread.
std::optional<std::filesystem::path> BuildJob::made_by(const BuildRequest& request, std::string& error) {
  try {
    return request.choice.build(
        request.game_dir, request.out_dir, [this](const std::string& step) { say_step(step); }, error);
  } catch (const std::exception& failure) {
    error = failure.what();
    return std::nullopt;
  }
}

void BuildJob::say_step(const std::string& step) {
  const std::lock_guard<std::mutex> hold(lock_);
  step_ = step;
}

std::string elapsed_text(double seconds) {
  const int whole = seconds > 0.0 ? static_cast<int>(seconds) : 0;
  const int hours = whole / kSecondsPerHour;
  const int minutes = whole % kSecondsPerHour / kSecondsPerMinute;
  const int rest = whole % kSecondsPerMinute;
  char text[kElapsedTextSize];
  if (hours > 0) {
    std::snprintf(text, sizeof(text), "%d:%02d:%02d", hours, minutes, rest);
  } else {
    std::snprintf(text, sizeof(text), "%d:%02d", minutes, rest);
  }
  return text;
}

}  // namespace redgem
