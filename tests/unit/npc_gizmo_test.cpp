// The NPC parts the editor finds in an object's properties, and the gizmo it draws of them.

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "editor/npc_gizmo.hpp"
#include "script/npc_settings.hpp"

using namespace redgem;

namespace {

constexpr float kTolerance = 1e-3f;
constexpr Vec3 kPlace{2.0f, 0.0f, -3.0f};

Transform at_place() {
  Transform world;
  world.position = kPlace;
  return world;
}

float flat_distance(Vec3 a, Vec3 b) { return length(Vec3{a.x - b.x, 0.0f, a.z - b.z}); }

std::size_t lines_of(const NpcGizmo& gizmo, NpcGizmoPart part) {
  return static_cast<std::size_t>(std::count_if(gizmo.lines.begin(), gizmo.lines.end(),
                                                [part](const NpcGizmoLine& line) { return line.part == part; }));
}

// Whether every line of a part ends `radius` from `center` across the ground.
bool ring_around(const NpcGizmo& gizmo, NpcGizmoPart part, Vec3 center, float radius) {
  return std::all_of(gizmo.lines.begin(), gizmo.lines.end(), [&](const NpcGizmoLine& line) {
    return line.part != part || std::fabs(flat_distance(line.to, center) - radius) < kTolerance;
  });
}

}  // namespace

TEST_CASE("an object's properties say which NPC parts it has") {
  CHECK_FALSE(npc_settings_from_props({}).interactable);
  CHECK_FALSE(npc_settings_from_props({{"speed", 3.0}}).perception);
  const NpcSettings door = npc_settings_from_props({{"prompt", std::string("door.open")}});
  REQUIRE(door.interactable);
  CHECK(door.interactable->range == doctest::Approx(kInteractableRange));
  CHECK(door.interactable->prompt == std::optional<std::string>("door.open"));
  const NpcSettings guard = npc_settings_from_props({{"sight", 8.0}, {"fov", std::string("wide")}});
  REQUIRE(guard.perception);
  CHECK(guard.perception->sight == doctest::Approx(8.0f));
  CHECK(guard.perception->fov == doctest::Approx(kPerceptionFov));  // not a number: the default
  CHECK(guard.perception->hearing == doctest::Approx(kPerceptionHearing));
}

TEST_CASE("a plan in text is its entries, trimmed, with no empty one") {
  CHECK(schedule_entries("06:00 bake, 12:30 sell ,20:00 sleep,") ==
        std::vector<std::string>{"06:00 bake", "12:30 sell", "20:00 sleep"});
  CHECK(schedule_entries("").empty());
  CHECK(npc_settings_from_props({{"plan", std::string("06:00 bake")}}).schedule ==
        std::vector<std::string>{"06:00 bake"});
}

TEST_CASE("an Interactable shows a ring of its range and its prompt over it") {
  NpcSettings settings;
  settings.interactable = InteractableSettings{};
  settings.interactable->range = 2.5f;
  const NpcGizmo gizmo = npc_gizmo(settings, at_place());
  CHECK(lines_of(gizmo, NpcGizmoPart::Interactable) == static_cast<std::size_t>(kNpcRingSegments));
  CHECK(ring_around(gizmo, NpcGizmoPart::Interactable, kPlace, 2.5f));
  REQUIRE(gizmo.labels.size() == 1);
  CHECK(gizmo.labels[0].text == kDefaultPromptKey);
  CHECK(gizmo.labels[0].at.y == doctest::Approx(kNpcPromptLift));
}

TEST_CASE("a Perception shows its sight cone from its eyes and its hearing ring") {
  NpcSettings settings;
  settings.perception = PerceptionSettings{};
  settings.perception->sight = 10.0f;
  settings.perception->fov = 90.0f;
  settings.perception->hearing = 4.0f;
  const NpcGizmo gizmo = npc_gizmo(settings, at_place());
  CHECK(ring_around(gizmo, NpcGizmoPart::Hearing, kPlace, 4.0f));
  CHECK(lines_of(gizmo, NpcGizmoPart::Sight) == static_cast<std::size_t>(kNpcArcSegments + 2));
  const Vec3 eyes = kPlace + Vec3::up() * kPerceptionEyes;
  for (const NpcGizmoLine& line : gizmo.lines) {
    if (line.part != NpcGizmoPart::Sight) continue;
    CHECK(flat_distance(line.to, eyes) == doctest::Approx(10.0f));
    CHECK(line.to.z < kPlace.z);  // in front: objects face -Z
  }
}

TEST_CASE("a Perception that sees all around shows a ring of its sight") {
  NpcSettings settings;
  settings.perception = PerceptionSettings{};
  settings.perception->fov = kNpcFullTurn;
  const NpcGizmo gizmo = npc_gizmo(settings, at_place());
  CHECK(lines_of(gizmo, NpcGizmoPart::Sight) == static_cast<std::size_t>(kNpcRingSegments));
}

TEST_CASE("a Schedule shows its plan, an entry a line") {
  NpcSettings settings;
  settings.schedule = {"06:00 bake", "20:00 sleep"};
  const NpcGizmo gizmo = npc_gizmo(settings, at_place());
  REQUIRE(gizmo.labels.size() == 1);
  CHECK(gizmo.labels[0].text == "06:00 bake\n20:00 sleep");
}

TEST_CASE("a NavAgent shows its path through the corners ahead and a cross where it goes") {
  NpcSettings settings;
  const Vec3 corner{4.0f, 0.0f, -3.0f};
  const Vec3 goal{4.0f, 0.0f, 1.0f};
  settings.route = NavRoute{goal, {corner, goal}};
  const NpcGizmo gizmo = npc_gizmo(settings, at_place());
  constexpr std::size_t kPathLines = 2;
  constexpr std::size_t kCrossLines = 3;
  REQUIRE(lines_of(gizmo, NpcGizmoPart::Route) == kPathLines + kCrossLines);
  CHECK(approx_equal(gizmo.lines[0].from, kPlace, kTolerance));
  CHECK(approx_equal(gizmo.lines[1].to, goal, kTolerance));
}

TEST_CASE("an object without NPC parts has no gizmo") {
  const NpcGizmo gizmo = npc_gizmo(NpcSettings{}, at_place());
  CHECK(gizmo.lines.empty());
  CHECK(gizmo.labels.empty());
}
