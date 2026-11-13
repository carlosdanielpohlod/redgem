#include <doctest/doctest.h>

#include "render/gpu_timers.hpp"

using namespace redgem;

TEST_CASE("a frame looks back at the sets the frames before it wrote, never at its own") {
  for (std::uint64_t frame = kGpuTimerFrames; frame < 3 * kGpuTimerFrames; ++frame) {
    CHECK(gpu_write_set(frame) < kGpuTimerFrames);
    CHECK(gpu_set_before(frame, 0) == gpu_write_set(frame));
    for (std::size_t age = 1; age < kGpuTimerFrames; ++age) {
      CHECK(gpu_set_before(frame, age) == gpu_write_set(frame - age));
      CHECK(gpu_set_before(frame, age) != gpu_write_set(frame));
    }
  }
}

TEST_CASE("the passes have names and the card's nanoseconds become milliseconds") {
  CHECK(gpu_pass_name(GpuPass::Prepare) == "prepare");
  CHECK(gpu_pass_name(GpuPass::Scene) == "scene");
  CHECK(gpu_pass_name(GpuPass::Post) == "post");
  CHECK(gpu_pass_name(GpuPass::Count).empty());
  CHECK(gpu_milliseconds(2500000) == doctest::Approx(2.5));
}

TEST_CASE("timers that are off measure nothing and touch no graphics context") {
  GpuTimers timers;
  CHECK_FALSE(timers.enabled());
  timers.start_frame();
  timers.begin(GpuPass::Scene);
  timers.end();
  CHECK_FALSE(timers.timings().measured);
}
