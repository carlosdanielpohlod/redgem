#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "editor/glb_file.hpp"
#include "editor/scene_document.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::scene_document_detail;
using redgem::test::kTolerance;

namespace {

// The tree of these tests:   Root(0) -> Arm(1) -> Hand(2),   Root -> Leg(3),   Lamp(4)
constexpr int kRoot = 0;
constexpr int kArm = 1;
constexpr int kHand = 2;
constexpr int kLeg = 3;
constexpr int kLamp = 4;
constexpr int kNodeCount = 5;
constexpr int kTwelfthCopy = 12;
constexpr int kThousandthCopy = 1000;
constexpr Vec3 kRootPlace{10.0f, 0.0f, 0.0f};
constexpr Vec3 kArmOffset{0.0f, 2.0f, 0.0f};
constexpr Vec3 kHandOffset{0.0f, 0.0f, 3.0f};
constexpr Vec3 kHandInWorld{10.0f, 2.0f, 3.0f};
constexpr std::size_t kChunkAlignment = 4;  // bytes: what a .glb pads its chunks to
constexpr std::size_t kUnaligned = 5;
constexpr std::size_t kNextAligned = 8;

NodeDesc node(const std::string& name, int parent, std::vector<int> children, Vec3 position = Vec3::zero()) {
  NodeDesc out;
  out.name = name;
  out.parent = parent;
  out.children = std::move(children);
  out.local.position = position;
  return out;
}

std::vector<NodeDesc> tree() {
  return {node("Root", kNoIndex, {kArm, kLeg}, kRootPlace), node("Arm", kRoot, {kHand}, kArmOffset),
          node("Hand", kArm, {}, kHandOffset), node("Leg", kRoot, {}), node("Lamp", kNoIndex, {})};
}

std::vector<std::string> names_of(const std::vector<NodeDesc>& nodes) {
  std::vector<std::string> names;
  for (const NodeDesc& each : nodes) names.push_back(each.name);
  return names;
}

}  // namespace

TEST_CASE("copies are numbered with three digits after a dot") {
  CHECK(numbered_name("Crate", 1) == "Crate.001");
  CHECK(numbered_name("Crate", kTwelfthCopy) == "Crate.012");
  CHECK(numbered_name("Crate", kThousandthCopy) == "Crate.1000");
}

TEST_CASE("a subtree lists an object and everything under it, parents first") {
  const std::vector<NodeDesc> nodes = tree();
  CHECK(subtree(nodes, kRoot) == std::vector<int>{kRoot, kArm, kHand, kLeg});
  CHECK(subtree(nodes, kArm) == std::vector<int>{kArm, kHand});
  CHECK(subtree(nodes, kLamp) == std::vector<int>{kLamp});
}

TEST_CASE("an object's place in the world adds up its parents' placements") {
  const std::vector<NodeDesc> nodes = tree();
  CHECK(approx_equal(world_of(nodes, kRoot).position, kRootPlace, kTolerance));
  CHECK(approx_equal(world_of(nodes, kHand).position, kHandInWorld, kTolerance));
}

TEST_CASE("an order can leave objects out or move them to the end") {
  CHECK(order_without(kNodeCount, {kArm, kHand}) == std::vector<int>{kRoot, kLeg, kLamp});
  CHECK(order_with_last(kNodeCount, {kArm, kHand}) == std::vector<int>{kRoot, kLeg, kLamp, kArm, kHand});
  CHECK(order_without(kNodeCount, {}).size() == kNodeCount);
}

TEST_CASE("removed objects leave their parent's children, and the others are renumbered") {
  const std::vector<NodeDesc> nodes = tree();
  const std::vector<NodeDesc> kept = nodes_in_order(nodes, order_without(kNodeCount, subtree(nodes, kArm)));
  CHECK(names_of(kept) == std::vector<std::string>{"Root", "Leg", "Lamp"});
  CHECK(kept[0].children == std::vector<int>{1});  // only the Leg is left, now second
  CHECK(kept[1].parent == 0);
  CHECK(kept.back().parent == kNoIndex);
}

TEST_CASE("objects moved to the end keep their parents and children by their new numbers") {
  const std::vector<NodeDesc> nodes = tree();
  const std::vector<NodeDesc> moved = nodes_in_order(nodes, order_with_last(kNodeCount, subtree(nodes, kArm)));
  CHECK(names_of(moved) == std::vector<std::string>{"Root", "Leg", "Lamp", "Arm", "Hand"});
  const int arm = kNodeCount - 2;
  const int hand = kNodeCount - 1;
  CHECK(moved[0].children == std::vector<int>{arm, 1});
  CHECK(moved[arm].parent == 0);
  CHECK(moved[arm].children == std::vector<int>{hand});
  CHECK(moved[hand].parent == arm);
}

TEST_CASE("chunk sizes of a .glb are rounded up to four bytes") {
  CHECK(glb_padded_size(0) == 0);
  CHECK(glb_padded_size(1) == kChunkAlignment);
  CHECK(glb_padded_size(kChunkAlignment) == kChunkAlignment);
  CHECK(glb_padded_size(kUnaligned) == kNextAligned);
}
