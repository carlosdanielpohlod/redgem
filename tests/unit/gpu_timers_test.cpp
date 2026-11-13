#include <doctest/doctest.h>

#include "render/gpu_timers.hpp"

using namespace redgem;

TEST_CASE("each frame reads back the set of queries the frame before wrote, never its own") {
  for (std::uint64_t frame = 1; frame < 6; ++frame) {
    CHECK(gpu_read_set(frame) == gpu_write_set(frame - 1));
    CHECK(gpu_read_set(frame) != gpu_write_set(frame));
    CHECK(gpu_write_set(frame) < kGpuTimerFrames);
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
