#include <doctest/doctest.h>

#include <limits>
#include <memory>
#include <stdexcept>

#include "core/frame_profiler.hpp"

using namespace redgem;

namespace {
struct Clock {
  double now = 0.0;
  int reads = 0;
  double read() {
    ++reads;
    return now;
  }
};
std::size_t column(ProfilePhase phase) { return static_cast<std::size_t>(phase); }
}  // namespace

TEST_CASE("disabled profiling never reads the clock and empty summaries are finite") {
  Clock clock;
  FrameProfiler profiler([&] { return clock.read(); });
  profiler.begin_frame();
  {
    const FrameProfileScope script(profiler, ProfilePhase::Update);
  }
  profiler.finish_frame();
  CHECK(clock.reads == 0);
  CHECK(profiler.summary().samples == 0);
  CHECK(profiler.summary().frames_per_second == 0.0);
}

TEST_CASE("profile phases accumulate, nest inclusively and use real time") {
  Clock clock;
  FrameProfiler profiler([&] { return clock.read(); });
  profiler.set_enabled(true);
  profiler.begin_frame();
  {
    const FrameProfileScope scripts(profiler, ProfilePhase::Update);
    clock.now = 0.002;
    {
      const FrameProfileScope physics(profiler, ProfilePhase::Physics);
      clock.now = 0.005;
    }
    clock.now = 0.008;
  }
  {
    const FrameProfileScope scripts(profiler, ProfilePhase::Update);
    clock.now = 0.010;
  }
  clock.now = 0.020;
  profiler.finish_frame();
  auto summary = profiler.summary();
  REQUIRE(summary.samples == 1);
  CHECK(summary.latest.frame_ms == doctest::Approx(20.0));
  CHECK(summary.latest.phase_ms[column(ProfilePhase::Update)] == doctest::Approx(10.0));
  CHECK(summary.latest.phase_ms[column(ProfilePhase::Physics)] == doctest::Approx(3.0));
  CHECK_FALSE(summary.latest.has_interval);
  CHECK(summary.frames_per_second == 0.0);
  clock.now = 0.025;
  profiler.begin_frame();
  clock.now = 0.035;
  profiler.finish_frame();
  summary = profiler.summary();
  CHECK(summary.mean.frame_ms == doctest::Approx(15.0));
  CHECK(summary.mean.interval_ms == doctest::Approx(15.0));
  CHECK(summary.frames_per_second == doctest::Approx(1000.0 / 15.0));
  CHECK(summary.peak.frame_ms == doctest::Approx(20.0));
}

TEST_CASE("the rolling profile window drops old peaks and stays bounded") {
  Clock clock;
  FrameProfiler profiler([&] { return clock.read(); });
  profiler.set_enabled(true);
  profiler.begin_frame();
  clock.now = 1.0;
  profiler.finish_frame();
  for (std::size_t i = 0; i < kProfileHistoryFrames; ++i) {
    clock.now += 0.010;
    profiler.begin_frame();
    clock.now += 0.002;
    profiler.finish_frame();
  }
  const auto summary = profiler.summary();
  CHECK(summary.samples == kProfileHistoryFrames);
  CHECK(summary.peak.frame_ms == doctest::Approx(2.0));
  CHECK(summary.mean.frame_ms == doctest::Approx(2.0));
}

TEST_CASE("disabled and cleared frames discard outstanding scopes") {
  Clock clock;
  FrameProfiler profiler([&] { return clock.read(); });
  profiler.set_enabled(true);
  profiler.begin_frame();
  auto stale = std::make_unique<FrameProfileScope>(profiler, ProfilePhase::Update);
  profiler.set_enabled(false);
  CHECK(profiler.enabled() == false);
  clock.now = 100.0;
  profiler.set_enabled(true);
  profiler.begin_frame();
  stale.reset();
  profiler.clear();
  profiler.begin_frame();
  clock.now += 0.005;
  profiler.finish_frame();
  CHECK(profiler.summary().samples == 1);
  CHECK_FALSE(profiler.summary().latest.has_interval);
  CHECK(profiler.summary().latest.phase_ms[column(ProfilePhase::Update)] == 0.0);
}

TEST_CASE("disabling retains the last summary and re-enabling starts a new session") {
  Clock clock;
  FrameProfiler profiler([&] { return clock.read(); });
  profiler.set_enabled(true);
  profiler.begin_frame();
  clock.now = 0.010;
  profiler.finish_frame();
  profiler.set_enabled(false);
  const int reads = clock.reads;
  CHECK(profiler.summary().samples == 1);
  profiler.begin_frame();
  profiler.finish_frame();
  CHECK(clock.reads == reads);
  profiler.set_enabled(true);
  CHECK(profiler.summary().samples == 0);
}

TEST_CASE("invalid clock readings and invalid phase ids cannot yield invalid metrics") {
  Clock clock;
  FrameProfiler profiler([&] { return clock.read(); });
  profiler.set_enabled(true);
  clock.now = std::numeric_limits<double>::quiet_NaN();
  profiler.begin_frame();
  profiler.finish_frame();
  CHECK(profiler.summary().samples == 0);
  clock.now = 1.0;
  profiler.begin_frame();
  {
    const FrameProfileScope invalid(profiler, ProfilePhase::Count);
  }
  clock.now = 0.0;
  profiler.finish_frame();
  CHECK(profiler.summary().latest.frame_ms == 0.0);
  CHECK(profile_phase_name(ProfilePhase::Count).empty());
}

TEST_CASE("profile scopes unwind on errors and snapshots expose completed frames only") {
  Clock clock;
  FrameProfiler profiler([&] { return clock.read(); });
  profiler.set_enabled(true);
  profiler.begin_frame();
  try {
    const FrameProfileScope phase(profiler, ProfilePhase::Update);
    clock.now = 0.003;
    throw std::runtime_error("callback failed");
  } catch (const std::runtime_error&) {
  }
  CHECK(profiler.summary().samples == 0);
  clock.now = 0.005;
  profiler.finish_frame();
  CHECK(profiler.summary().latest.phase_ms[column(ProfilePhase::Update)] == doctest::Approx(3.0));
  profiler.begin_frame();
  profiler.set_enabled(true);  // repeated assignments do not reset an active session
  CHECK(profiler.summary().samples == 1);
  clock.now = 0.010;
  profiler.finish_frame();
  CHECK(profiler.summary().samples == 2);
}

TEST_CASE("a scope outliving its frame cannot contaminate the next frame") {
  Clock clock;
  FrameProfiler profiler([&] { return clock.read(); });
  profiler.set_enabled(true);
  profiler.begin_frame();
  auto stale = std::make_unique<FrameProfileScope>(profiler, ProfilePhase::Physics);
  clock.now = 0.010;
  profiler.finish_frame();
  profiler.begin_frame();
  stale.reset();
  clock.now = 0.020;
  profiler.finish_frame();
  CHECK(profiler.summary().latest.phase_ms[column(ProfilePhase::Physics)] == 0.0);
  clock.now = 0.030;
  profiler.begin_frame();
  clock.now = std::numeric_limits<double>::infinity();
  profiler.finish_frame();
  CHECK(profiler.summary().samples == 2);
}
