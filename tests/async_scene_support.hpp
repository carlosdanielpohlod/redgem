#pragma once
#include <doctest/doctest.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include "app/scene_loader.hpp"
#include "core/log.hpp"
namespace redgem::test {
inline constexpr auto kSceneWaitLimit = std::chrono::seconds(10);
// The most a test waits for another thread to read a scene: a bound for a reader that died, never a
// measure (how long a read takes is the computer's).
inline constexpr float kSceneReadPatience = 10.0f;
// Frames a scene read takes at most to be made ready, once read (see kActivationSecondsPerFrame).
inline constexpr int kMostActivationFrames = 600;
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
