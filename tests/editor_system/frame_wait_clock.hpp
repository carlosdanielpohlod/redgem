#pragma once

#include <doctest/doctest.h>
#include <chrono>
#include <future>
#include <mutex>
#include <optional>

#include "editor/editor_app.hpp"
#include "project/editor_bridge.hpp"

namespace redgem::test {
// Network deadlines advance normally between frames. Software-rendered frames are outside the
// transport test's budget, while an independent real-time guard still bounds a stalled test.
class FrameWaitClock {
  using Clock = std::chrono::steady_clock;
  static constexpr auto kTestGuard = std::chrono::minutes(5);
  mutable std::mutex mutex_;
  std::optional<Clock::time_point> paused_;
  Clock::duration excluded_{};
  void pause() {
    const std::lock_guard<std::mutex> hold(mutex_);
    paused_ = Clock::now();
  }
  void resume() {
    const std::lock_guard<std::mutex> hold(mutex_);
    excluded_ += Clock::now() - *paused_;
    paused_.reset();
  }
  Clock::time_point now() const {
    const std::lock_guard<std::mutex> hold(mutex_);
    return paused_.value_or(Clock::now()) - excluded_;
  }
  struct Pause {
    FrameWaitClock& clock;
    explicit Pause(FrameWaitClock& value) : clock(value) { clock.pause(); }
    ~Pause() { clock.resume(); }
  };

 public:
  tools::EditorBridgeClock reading() {
    return [this] { return now(); };
  }
  template <typename T>
  void pump(EditorApp& app, std::future<T>& reply) {
    const auto deadline = Clock::now() + kTestGuard;
    while (reply.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
      REQUIRE(Clock::now() < deadline);
      const Pause pause(*this);
      app.frame();
    }
  }
};
}  // namespace redgem::test
