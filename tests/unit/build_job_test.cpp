#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/log.hpp"
#include "editor/build_job.hpp"

using namespace redgem;

namespace {

constexpr const char* kTarget = "android";
constexpr const char* kStep = "signing the APK";
constexpr const char* kLogged = "the build has something to say";
constexpr const char* kWhyNot = "no Android SDK";
constexpr auto kPatience = std::chrono::seconds(10);  // far longer than any step here takes
constexpr auto kShortWait = std::chrono::milliseconds(50);
// Times of builds, in seconds, and how they read.
constexpr double kNearlyEight = 7.9;
constexpr double kTwelveAndAHalfMinutes = 750.0;
constexpr double kHourTwoMinutesThree = 3723.0;
constexpr double kBeforeTheStart = -2.0;

const std::filesystem::path kGame = "games/mine";
const std::filesystem::path kOut = "games/mine-build-android";
const std::filesystem::path kMade = kOut / "mine.apk";

// A build that says a step, logs a line, then waits to be let go before it ends as `outcome` says.
struct HeldBuild {
  std::promise<void> release;
  std::shared_future<void> released = release.get_future().share();
  std::promise<void> stepped;
  std::future<void> said_step = stepped.get_future();
  std::thread::id thread;           // the one it ran on
  std::filesystem::path game_seen;  // the game it was asked to build

  BuildRequest request() {
    BuildRequest request;
    request.choice.name = kTarget;
    request.choice.build = [this](const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                                  const BuildProgress& progress, std::string&) -> std::optional<std::filesystem::path> {
      thread = std::this_thread::get_id();
      game_seen = game_dir;
      progress(kStep);
      log::info(kLogged);
      stepped.set_value();
      released.wait_for(kPatience);  // (a test that fails before letting it go does not hang)
      return out_dir / kMade.filename();
    };
    request.game_dir = kGame;
    request.out_dir = kOut;
    return request;
  }
};

// A build that fails at once, in the way `fail` does.
BuildRequest failing(const std::function<void(std::string& error)>& fail) {
  BuildRequest request;
  request.choice.name = kTarget;
  request.choice.build = [fail](const std::filesystem::path&, const std::filesystem::path&, const BuildProgress&,
                                std::string& error) -> std::optional<std::filesystem::path> {
    fail(error);
    return std::nullopt;
  };
  return request;
}

// What the job says once its build ended.
BuildOutcome ended(BuildJob& job) {
  job.wait();
  std::optional<BuildOutcome> outcome = job.take_outcome();
  REQUIRE(outcome.has_value());
  return *outcome;
}

}  // namespace

TEST_CASE("a build runs on a thread of its own, says its step, and hands over how it ended once") {
  HeldBuild held;
  BuildJob job;
  REQUIRE(job.start(held.request()));
  CHECK(job.busy());
  CHECK(job.target() == kTarget);
  REQUIRE(held.said_step.wait_for(kPatience) == std::future_status::ready);
  CHECK(job.step() == kStep);
  CHECK_FALSE(job.take_outcome().has_value());  // it still runs
  CHECK(job.busy());
  held.release.set_value();

  const BuildOutcome outcome = ended(job);
  CHECK(held.thread != std::this_thread::get_id());
  CHECK(held.game_seen == kGame);
  CHECK(outcome.succeeded());
  CHECK(outcome.target == kTarget);
  CHECK(outcome.out_dir == kOut);
  CHECK(*outcome.made == kMade);
  CHECK(outcome.seconds >= 0.0);
  CHECK_FALSE(job.busy());
  CHECK_FALSE(job.take_outcome().has_value());  // taken already
}

TEST_CASE("what a build logs waits in its outcome instead of reaching the log from the build's thread") {
  std::vector<std::string> sunk;
  const log::Sink previous = log::set_sink([&sunk](log::Level, const std::string& text) { sunk.push_back(text); });
  HeldBuild held;
  BuildJob job;
  REQUIRE(job.start(held.request()));
  held.release.set_value();
  const BuildOutcome outcome = ended(job);
  log::set_sink(previous);
  CHECK(sunk.empty());
  REQUIRE(outcome.messages.size() == 1);
  CHECK(outcome.messages.front().level == log::Level::Info);
  CHECK(outcome.messages.front().text == kLogged);
}

TEST_CASE("only one build runs at a time: another starts once the first one's outcome is taken") {
  HeldBuild first;
  BuildJob job;
  REQUIRE(job.start(first.request()));
  HeldBuild second;
  CHECK_FALSE(job.start(second.request()));
  first.release.set_value();
  job.wait();
  CHECK_FALSE(job.start(second.request()));  // ended, but not taken in yet
  CHECK(job.take_outcome().has_value());
  CHECK(job.start(second.request()));
  second.release.set_value();
  CHECK(ended(job).succeeded());
}

TEST_CASE("a build that fails says why, and one that throws fails with what it threw") {
  BuildJob job;
  REQUIRE(job.start(failing([](std::string& error) { error = kWhyNot; })));
  BuildOutcome outcome = ended(job);
  CHECK_FALSE(outcome.succeeded());
  CHECK(outcome.error == kWhyNot);

  REQUIRE(job.start(failing([](std::string&) { throw std::runtime_error(kWhyNot); })));
  outcome = ended(job);
  CHECK_FALSE(outcome.succeeded());
  CHECK(outcome.error == kWhyNot);
}

TEST_CASE("a job that goes while its build runs waits for the build: its thread never outlives it") {
  std::atomic<bool> finished{false};
  {
    BuildJob job;
    BuildRequest request = failing([&finished](std::string&) {
      std::this_thread::sleep_for(kShortWait);
      finished = true;
    });
    REQUIRE(job.start(std::move(request)));
  }
  CHECK(finished);
}

TEST_CASE("a build's time counts up from its start") {
  HeldBuild held;
  BuildJob job;
  REQUIRE(job.start(held.request()));
  const double early = job.seconds();
  std::this_thread::sleep_for(kShortWait);
  CHECK(job.seconds() > early);
  held.release.set_value();
  CHECK(ended(job).seconds > 0.0);
}

TEST_CASE("a build's time reads in minutes and seconds, and hours once it takes that long") {
  CHECK(elapsed_text(0.0) == "0:00");
  CHECK(elapsed_text(kNearlyEight) == "0:07");
  CHECK(elapsed_text(kTwelveAndAHalfMinutes) == "12:30");
  CHECK(elapsed_text(kHourTwoMinutesThree) == "1:02:03");
  CHECK(elapsed_text(kBeforeTheStart) == "0:00");
}
