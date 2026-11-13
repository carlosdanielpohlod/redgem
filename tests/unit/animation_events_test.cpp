// Animation events: the moments of a character's own clips that scripts listen for, met once on
// each pass however the clips are played.

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "scene/animation/animation_player.hpp"

using namespace redgem;

namespace {

constexpr ObjectId kHero = 0;
constexpr ObjectId kBone = 1;
constexpr float kClipSeconds = 1.0f;
constexpr float kFrame = 1.0f / 60.0f;
constexpr int kFramesPerClip = 60;
constexpr float kQuarter = 0.25f;
constexpr float kHalf = 0.5f;
constexpr float kThreeQuarters = 0.75f;
constexpr float kLight = 0.4f;  // a mix too light to fire events
constexpr float kHeavy = 0.6f;  // and one heavy enough

AnimationDesc clip(const std::string& name, bool moving = true) {
  AnimationChannel channel;
  channel.node = kBone;
  channel.times = {0.0f, kClipSeconds};
  channel.values = {0, 0, 0, moving ? 1.0f : 0.0f, 0, 0};
  return {name, kClipSeconds, {channel}, {}};
}

// A hero with one bone, and clips that move it.
struct Stage {
  SceneDesc desc;
  Scene scene;
  AnimationPlayer player;

  Stage() {
    desc.nodes.resize(2);
    desc.nodes[kHero].name = "Hero";
    desc.nodes[kHero].children = {kBone};
    desc.nodes[kBone].name = "Bone";
    desc.nodes[kBone].parent = kHero;
    desc.animations = {clip("Walk"), clip("Run"), clip("Wave", false), clip("Attack")};
    desc.animations[0].markers = {{"footstep", kQuarter}, {"footstep", kThreeQuarters}, {"breath", kHalf}};
    scene = Scene::from_desc(desc);
    player.reset(desc.animations, scene);
  }

  void play(const std::string& name, float speed = 1.0f) {
    REQUIRE(player.play_on(scene, kHero, name, {true, speed, 0.0f, true}));
  }
  std::uint32_t on(const std::string& name, float at) {
    const std::uint32_t listener = player.listen_at(scene, kHero, name, at);
    REQUIRE(listener != kNoAnimationListener);
    return listener;
  }
  // The clip and moment of each event since the last call.
  std::vector<std::string> heard(float dt) {
    player.advance(dt, scene);
    std::vector<std::string> out;
    for (const AnimationEvent& event : player.take_events()) out.push_back(event.clip + "@" + std::to_string(event.at));
    return out;
  }
  std::size_t count_over(int frames) {
    std::size_t count = 0;
    for (int i = 0; i < frames; ++i) count += heard(kFrame).size();
    return count;
  }
};

std::string at(const std::string& clip, float moment) { return clip + "@" + std::to_string(moment); }

using Heard = std::vector<std::string>;

}  // namespace

TEST_CASE("an event fires when its clip passes its moment, once on every lap") {
  Stage stage;
  stage.play("Walk");
  const std::uint32_t listener = stage.on("Walk", kHalf);
  CHECK(stage.heard(0.4f).empty());
  stage.player.advance(0.2f, stage.scene);
  const std::vector<AnimationEvent> events = stage.player.take_events();
  REQUIRE(events.size() == 1);
  CHECK(events[0].listener == listener);
  CHECK(events[0].object == stage.scene.handle(kHero));
  CHECK(events[0].clip == "Walk");
  CHECK(events[0].name.empty());
  CHECK(events[0].at == doctest::Approx(kHalf));
  CHECK(stage.count_over(kFramesPerClip * 3) == 3u);
}

TEST_CASE("a loop that wraps round fires the moments past its end and after its start") {
  Stage stage;
  stage.play("Walk");
  stage.on("Walk", 0.95f);
  stage.on("Walk", 0.0f);
  CHECK(stage.heard(0.9f) == Heard{at("Walk", 0.0f)});  // its start, as it starts
  CHECK(stage.heard(0.2f) == Heard{at("Walk", 0.95f), at("Walk", 0.0f)});
  CHECK(stage.heard(0.2f).empty());
}

TEST_CASE("a large step fires every moment it skipped, in order") {
  Stage stage;
  stage.play("Walk");
  stage.on("Walk", kThreeQuarters);
  stage.on("Walk", kQuarter);
  CHECK(stage.heard(2.0f) ==
        Heard{at("Walk", kQuarter), at("Walk", kThreeQuarters), at("Walk", kQuarter), at("Walk", kThreeQuarters)});
}

TEST_CASE("two moments in one frame fire in the order the clip plays them") {
  Stage stage;
  stage.play("Walk");
  stage.on("Walk", 0.45f);
  stage.on("Walk", 0.4f);
  CHECK(stage.heard(0.3f).empty());
  CHECK(stage.heard(0.2f) == Heard{at("Walk", 0.4f), at("Walk", 0.45f)});
}

TEST_CASE("played backwards, moments fire from the end towards the start") {
  Stage stage;
  stage.play("Walk", -1.0f);
  stage.on("Walk", kQuarter);
  stage.on("Walk", kThreeQuarters);
  CHECK(stage.heard(kHalf) == Heard{at("Walk", kThreeQuarters)});
  CHECK(stage.heard(kHalf) == Heard{at("Walk", kQuarter)});
}

TEST_CASE("markers authored with a clip fire by their name") {
  Stage stage;
  stage.play("Walk");
  REQUIRE(stage.player.listen_marker(stage.scene, kHero, "footstep") != kNoAnimationListener);
  stage.player.advance(kClipSeconds - kFrame, stage.scene);
  const std::vector<AnimationEvent> events = stage.player.take_events();
  REQUIRE(events.size() == 2);
  CHECK(events[0].name == "footstep");
  CHECK(events[0].at == doctest::Approx(kQuarter));
  CHECK(events[1].at == doctest::Approx(kThreeQuarters));
}

TEST_CASE("a listener taken off hears nothing more") {
  Stage stage;
  stage.play("Walk");
  stage.player.unlisten(stage.on("Walk", kHalf));
  CHECK(stage.count_over(kFramesPerClip * 2) == 0u);
  CHECK(stage.player.listen_at(stage.scene, kHero, "Dance", kHalf) == kNoAnimationListener);
}

TEST_CASE("a character that only follows the scene's clips fires no events") {
  Stage stage;
  stage.on("Walk", kHalf);
  CHECK(stage.count_over(kFramesPerClip * 2) == 0u);
}

TEST_CASE("in a blend, only the clip with the most weight fires; a mix fires when heavy enough") {
  Stage stage;
  AnimationGraphDefinition graph;
  graph.initial = "Move";
  graph.parameters = {{"speed", kQuarter}};
  graph.states = {{"Move", "speed", {{0.0f, "Walk"}, {1.0f, "Run"}}}};
  std::string error;
  REQUIRE(stage.player.use_graph(stage.scene, kHero, graph, error));
  stage.on("Walk", 0.1f);
  stage.on("Run", 0.1f);
  stage.on("Wave", 0.1f);
  REQUIRE(stage.player.mix_on(stage.scene, kHero, "Wave", kLight, true, 1.0f));
  CHECK(stage.heard(0.2f) == Heard{at("Walk", 0.1f)});
  REQUIRE(stage.player.set_parameter(stage.scene, kHero, "speed", kThreeQuarters));
  REQUIRE(stage.player.mix_on(stage.scene, kHero, "Wave", kHeavy, true, 1.0f));
  CHECK(stage.heard(kClipSeconds) == Heard{at("Run", 0.1f), at("Wave", 0.1f)});
}

TEST_CASE("a state transition fires the new state's moments from its start, and no more of the old one's") {
  Stage stage;
  AnimationGraphDefinition graph;
  graph.initial = "Move";
  graph.parameters = {{"attack", false}};
  graph.states = {{"Move", "", {{0.0f, "Walk"}}}, {"Strike", "", {{0.0f, "Attack"}}, false}};
  graph.transitions = {{"Move", "Strike", "attack", AnimationComparison::Equal, true, kQuarter}};
  std::string error;
  REQUIRE(stage.player.use_graph(stage.scene, kHero, graph, error));
  stage.on("Walk", kHalf);
  stage.on("Attack", 0.0f);
  stage.on("Attack", 0.1f);
  CHECK(stage.heard(0.3f).empty());
  REQUIRE(stage.player.set_parameter(stage.scene, kHero, "attack", true));
  CHECK(stage.heard(0.3f) == Heard{at("Attack", 0.0f), at("Attack", 0.1f)});
  // Played once, it fires its last moment as it ends, and nothing while it holds there.
  stage.on("Attack", 1.0f);
  CHECK(stage.heard(kClipSeconds) == Heard{at("Attack", 1.0f)});
  CHECK(stage.heard(kClipSeconds).empty());
}
