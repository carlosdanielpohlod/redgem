#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "render/frame_pacer.hpp"

using namespace redgem;

namespace {

// Marks numbered from 1, and what was done with them, in order.
struct FenceLog {
  std::uintptr_t next = 1;
  std::vector<std::uintptr_t> waited;
  std::vector<std::uintptr_t> released;

  FenceCalls calls() {
    return {[this] { return reinterpret_cast<render_detail::CardFence>(next++); },
            [this](render_detail::CardFence fence) { waited.push_back(reinterpret_cast<std::uintptr_t>(fence)); },
            [this](render_detail::CardFence fence) { released.push_back(reinterpret_cast<std::uintptr_t>(fence)); }};
  }
};

}  // namespace

TEST_CASE("frames drawn off screen never run more than a few ahead of the card") {
  FenceLog log;
  FramePacer pacer(2, log.calls());
  pacer.frame_drawn();
  pacer.frame_drawn();
  CHECK(pacer.in_flight() == 2);
  CHECK(log.waited.empty());
  for (int frame = 0; frame < 10; ++frame) {
    pacer.frame_drawn();
    CHECK(pacer.in_flight() == 2);
  }
  CHECK(log.waited.size() == 10);
  CHECK(log.waited.front() == 1);  // the oldest mark first
  CHECK(log.waited.back() == 10);
  CHECK(log.released == log.waited);
}

TEST_CASE("the pacer lets go of the marks still in flight when it goes") {
  FenceLog log;
  {
    FramePacer pacer(3, log.calls());
    for (int frame = 0; frame < 5; ++frame) pacer.frame_drawn();
  }
  CHECK(log.waited.size() == 2);
  CHECK(log.released.size() == 5);
}

TEST_CASE("a pacer asked for no frames ahead still lets the card have one") {
  FenceLog log;
  FramePacer pacer(0, log.calls());
  pacer.frame_drawn();
  CHECK(pacer.in_flight() == 1);
  pacer.frame_drawn();
  CHECK(pacer.in_flight() == 1);
  CHECK(log.waited.size() == 1);
}
