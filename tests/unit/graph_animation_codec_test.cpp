#include <doctest/doctest.h>

#include <limits>

#include "net/pose_codec.hpp"

using namespace redgem::net;

namespace {

AnimationPart graph_part() {
  AnimationPart part;
  GraphPart graph;
  graph.from = {{{"Walk", 0.5f, 1.0f, true, true}, 1.0f}};
  graph.to = {{{"Run", 0.25f, 2.0f, true, true}, 0.5f}, {{"Idle", 0.25f, 1.0f, true, true}, 0.5f}};
  graph.blend = 0.25f;
  graph.remaining = 0.75f;
  part.graph = graph;
  return part;
}

PoseUpdate graph_pose(const AnimationPart& part) {
  PoseUpdate update;
  update.key = "Hero";
  update.animation = part;
  return update;
}

}  // namespace

TEST_CASE("evaluated animation graph tracks survive a pose wire round trip") {
  const auto update = decoded(encoded(graph_pose(graph_part())));
  REQUIRE(update.has_value());
  REQUIRE(update->animation->graph.has_value());
  const auto& graph = *update->animation->graph;
  CHECK(graph.blend == 0.25f);
  CHECK(graph.remaining == 0.75f);
  CHECK(graph.from.front().clip.clip == "Walk");
  REQUIRE(graph.to.size() == 2);
  CHECK(graph.to.front().weight == 0.5f);
  CHECK(graph.to.front().clip.time == 0.25f);
  CHECK(graph.to.front().clip.speed == 2.0f);
}

TEST_CASE("invalid graph weights clocks sizes and mixed modes are rejected on the wire") {
  auto part = graph_part();
  SUBCASE("too many tracks") { part.graph->to.push_back(part.graph->to.front()); }
  SUBCASE("no destination") { part.graph->to.clear(); }
  SUBCASE("weights do not sum to one") { part.graph->to[0].weight = 0.25f; }
  SUBCASE("negative weight") { part.graph->to[0].weight = -0.5f; }
  SUBCASE("nonfinite weight") { part.graph->to[0].weight = std::numeric_limits<float>::infinity(); }
  SUBCASE("negative time") { part.graph->to[0].clip.time = -1.0f; }
  SUBCASE("nonfinite speed") { part.graph->to[0].clip.speed = std::numeric_limits<float>::quiet_NaN(); }
  SUBCASE("invalid blend") { part.graph->blend = 2.0f; }
  SUBCASE("missing fade") { part.graph->remaining = 0.0f; }
  SUBCASE("missing source") { part.graph->from.clear(); }
  SUBCASE("manual base and graph") { part.own = ClipPart{"Walk"}; }
  CHECK_FALSE(carriable(part));
  CHECK_FALSE(decoded(encoded(graph_pose(part))).has_value());
}

TEST_CASE("truncated graph bytes and unknown playing flags are rejected") {
  const auto bytes = encoded(graph_pose(graph_part()));
  const auto manual = encoded(graph_pose(AnimationPart{})).size();
  for (std::size_t size = manual + 1; size < bytes.size(); ++size)
    CHECK_FALSE(decoded({bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size)}).has_value());
  auto flags = bytes;
  flags[manual + kGraphHeadBytes - 1] = 2;
  CHECK_FALSE(decoded(flags).has_value());
}

TEST_CASE("the largest graph pose fits the declared wire limit exactly") {
  auto part = graph_part();
  const ClipPart clip{std::string(kMaxClipNameBytes, 'x'), 0.0f, 1.0f, true, true};
  part.layer = clip;
  part.graph->from = {{clip, 0.5f}, {clip, 0.5f}};
  part.graph->to = part.graph->from;
  auto update = graph_pose(part);
  update.key = std::string(kMaxPoseKeyBytes, 'x');
  update.position = redgem::Vec3::zero();
  update.rotation = 0;
  CHECK(carriable(part));
  CHECK(encoded(update).size() == kMaxPoseBytes);
  CHECK(decoded(encoded(update)).has_value());
}
