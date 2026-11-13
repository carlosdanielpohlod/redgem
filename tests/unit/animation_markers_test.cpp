#include <doctest/doctest.h>

#include <vector>

#include "scene/animation/animation_markers.hpp"

using namespace redgem;

namespace {

constexpr double kClip = 2.0;   // seconds
constexpr double kEarly = 0.5;  // a moment a quarter into the clip
constexpr double kLate = 1.5;   // three quarters into it
constexpr double kFrame = 0.1;  // seconds of one frame
constexpr int kTwoClips = 40;   // frames: twice round the clip
const std::vector<double> kMarks{kEarly, kLate};

using Order = std::vector<std::size_t>;

ClipSpan looping(double from, double to) { return {from, to, kClip, true}; }
ClipSpan once(double from, double to) { return {from, to, kClip, false}; }

}  // namespace

TEST_CASE("a frame meets the moments between where it starts and where it ends, once each") {
  CHECK(marks_passed(looping(0.4, 0.6), kMarks) == Order{0});
  CHECK(marks_passed(looping(0.6, 1.4), kMarks).empty());
  // Where a frame ends, the next one starts: the moment there is met by the next frame.
  CHECK(marks_passed(looping(0.3, kEarly), kMarks).empty());
  CHECK(marks_passed(looping(kEarly, 0.7), kMarks) == Order{0});
}

TEST_CASE("playing frame after frame meets each moment once on every lap") {
  std::vector<std::size_t> met;
  double time = 0.0;
  for (int frame = 0; frame < kTwoClips; ++frame) {
    const double next = time + kFrame;
    for (const std::size_t mark : marks_passed(looping(time, next), kMarks)) met.push_back(mark);
    time = next;
  }
  CHECK(met == Order{0, 1, 0, 1});
}

TEST_CASE("a looping clip that wraps round meets the moments past its end and after its start") {
  // From 1.4 s on past the end (2.0) to 0.6 s of the next lap.
  CHECK(marks_passed(looping(1.4, kClip + 0.6), kMarks) == Order{1, 0});
}

TEST_CASE("a long frame meets every moment it skipped, in order, lap after lap") {
  CHECK(marks_passed(looping(0.0, kClip * 2.0), kMarks) == Order{0, 1, 0, 1});
  CHECK(marks_passed(looping(0.0, kClip * 1000.0), kMarks).size() == kMarks.size() * kMaxMarkerLaps);
}

TEST_CASE("two moments in one frame come in the order the clip plays them") {
  const std::vector<double> close{0.7, 0.6};
  CHECK(marks_passed(looping(0.5, 0.8), close) == Order{1, 0});
}

TEST_CASE("played backwards, moments come from the end towards the start") {
  CHECK(marks_passed(looping(1.8, 0.2), kMarks) == Order{1, 0});
  // Backwards round the start of the clip and on from its end.
  CHECK(marks_passed(looping(0.6, -0.6), kMarks) == Order{0, 1});
}

TEST_CASE("a clip played once meets its last moment when it gets there, and nothing after") {
  const std::vector<double> ends{0.0, kClip};
  CHECK(marks_passed(once(1.9, kClip), ends) == Order{1});
  CHECK(marks_passed(once(kClip, kClip), ends).empty());  // holding its last pose
  CHECK(marks_passed(once(0.0, 0.1), ends) == Order{0});
  CHECK(marks_passed(once(0.1, 0.0), ends) == Order{0});  // backwards to its start
}

TEST_CASE("markers are read from name@time entries") {
  const ParsedMarkers parsed = parse_animation_markers(" footstep@0.25, footstep @ 0.75,hit@1, bad, far@1.5, @0.5,");
  REQUIRE(parsed.markers.size() == 3);
  CHECK(parsed.markers[0].name == "footstep");
  CHECK(parsed.markers[0].at == doctest::Approx(0.25f));
  CHECK(parsed.markers[1].name == "footstep");
  CHECK(parsed.markers[1].at == doctest::Approx(0.75f));
  CHECK(parsed.markers[2].name == "hit");
  CHECK(parsed.markers[2].at == doctest::Approx(1.0f));
  CHECK(parsed.refused == std::vector<std::string>{"bad", "far@1.5", "@0.5"});
}
