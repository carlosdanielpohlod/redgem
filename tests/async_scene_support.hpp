#pragma once
#include <doctest/doctest.h>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include "app/scene_loader.hpp"
#include "core/log.hpp"
#include "core/stopwatch.hpp"
namespace redgem::test {
inline constexpr auto kSceneWaitLimit = std::chrono::seconds(10);
// The most a test waits for another thread to read a scene: a bound for a reader that died, never a
// measure (how long a read takes is the computer's).
inline constexpr float kSceneReadPatience = 10.0f;
// Frames a scene read takes at most to be made ready, once read, on an activation clock that moves
// kActivationTick each time it is read (see EngineOptions::activation_time).
inline constexpr int kMostActivationFrames = 600;
inline constexpr double kActivationTick = 0.001;

// That clock: each reading a tick later than the one before, however fast the computer is.
inline Stopwatch::Reading ticking_activation_clock() {
  return [ticks = std::make_shared<double>(0.0)] { return *ticks += kActivationTick; };
}
// A parser held at a condition variable, with RAII release even after a failed assertion.
class HeldSceneReader {
  std::mutex mutex_;
  std::condition_variable changed_;
  bool entered_ = false;
  bool released_ = false;

 public:
  HeldSceneReader() : loader([this](const auto& game, const auto& name) { return read(game, name); }) {}
  ~HeldSceneReader() { release(); }
  void release() {
    const std::lock_guard<std::mutex> hold(mutex_);
    released_ = true;
    changed_.notify_all();
  }
  void wait_started() {
    std::unique_lock<std::mutex> hold(mutex_);
    REQUIRE(changed_.wait_for(hold, kSceneWaitLimit, [this] { return entered_; }));
  }
  SceneLoader loader;

 private:
  LoadResult read(const std::filesystem::path& game, const std::string& name) {
    {
      std::unique_lock<std::mutex> hold(mutex_);
      entered_ = true;
      changed_.notify_all();
      if (!changed_.wait_for(hold, kSceneWaitLimit, [this] { return released_; }))
        throw std::runtime_error("test reader timed out");
    }
    log::warning("background scene diagnostic");
    return read_scene_data(game, name);
  }
};

}  // namespace redgem::test
