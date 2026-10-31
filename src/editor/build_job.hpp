#pragma once

// A build of the game on a thread of its own, so that the editor keeps drawing while it works (a
// build for Android takes minutes). The editor's thread starts it, asks how it goes every frame
// and takes how it ended once it has. The build's thread works only on the files on disk and on
// its own copy of what it was asked: never on the scene being edited, nor on the editor's console
// (what the build logs is kept, and handed over with how it ended).

#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "core/log.hpp"
#include "editor/editor_options.hpp"

namespace redgem {

// What to build: the game in `game_dir`, into `out_dir`, in one of the editor's ways.
struct BuildRequest {
  BuildChoice choice;
  std::filesystem::path game_dir;
  std::filesystem::path out_dir;
};

// A message the build logged, for the editor's console.
struct BuildMessage {
  log::Level level = log::Level::Info;
  std::string text;
};

// How a build ended.
struct BuildOutcome {
  std::string target;  // the system built for ("android")
  std::filesystem::path out_dir;
  std::optional<std::filesystem::path> made;  // the game's executable (or app), when it was made
  std::string error;                          // why not, when it was not
  double seconds = 0.0;                       // how long it took
  std::vector<BuildMessage> messages;         // what it logged, in order
  bool succeeded() const { return made.has_value(); }
};

class BuildJob {
 public:
  BuildJob() = default;
  // Waits for a build that still runs: its thread never outlives the job.
  ~BuildJob();
  BuildJob(const BuildJob&) = delete;
  BuildJob& operator=(const BuildJob&) = delete;

  // Starts the build on a thread of its own. False, and nothing started, while another one is
  // busy: one build at a time.
  bool start(BuildRequest request);
  // A build was started, and how it ended was not taken yet.
  bool busy() const { return busy_; }
  // The system the busy build is for.
  const std::string& target() const { return target_; }
  // The step the build said it began last; empty before the first.
  std::string step() const;
  // Seconds since the busy build started.
  double seconds() const;
  // How the build ended, once it has; after that the job is free for another. nullopt while the
  // build still runs, and when none was started.
  std::optional<BuildOutcome> take_outcome();
  // Waits until the build ends (at once when none runs).
  void wait();

 private:
  using Clock = std::chrono::steady_clock;

  // On the build's thread.
  void work(const BuildRequest& request);
  std::optional<std::filesystem::path> made_by(const BuildRequest& request, std::string& error);
  void say_step(const std::string& step);
  bool ended() const;

  // The editor's thread sets these before the build's thread starts, which only reads them.
  bool busy_ = false;
  std::string target_;
  Clock::time_point started_;
  mutable std::mutex lock_;              // guards the two below, which both threads use
  std::string step_;                     // what the build said it began last
  std::optional<BuildOutcome> outcome_;  // set by the build's thread as it ends
  std::thread worker_;
};

// "0:07", "12:30", "1:02:03": how long a build has taken, as the editor shows it.
std::string elapsed_text(double seconds);

}  // namespace redgem
