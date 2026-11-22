// Lag compensation across two machines: a guest reaches the host through a network that holds what
// it carries 50 ms each way (100 ms there and back), and shoots at a target the host moves, where
// the guest sees it. The host judges the shot against the hit box where it was when the guest saw
// it (Physics.rewind with the shot's view time) or where it is now; only the first hits. The view
// time comes with the message, and a rewind goes no further back than game.json says.

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "integration/middleman.hpp"
#include "integration/physics_game.hpp"
#include "socket_rounds.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;
using redgem::test::Middleman;

namespace {

constexpr int kOneWayRounds = 3;  // frames of 1/60 s: 50 ms each way
constexpr int kMaxRounds = 3000;
constexpr int kSettleRounds = 60;  // a second: the target is shown on the guest by then
constexpr int kShots = 5;          // one every kRoundsBetweenShots frames
constexpr int kRoundsBetweenShots = 20;
constexpr int kShortRewindMs = 50;  // a cap well under what the guest is behind

// The hit box layer meets nothing: rays find it, bodies pass through it.
std::string game_json(int max_rewind_ms) {
  return R"({"title": "Range", "physics": {"layers": ["default", "hitbox"], "collides": {"hitbox": []}},
             "network": {"max_rewind_ms": )" +
         std::to_string(max_rewind_ms) + "}}";
}

// A target with a one-meter hit box a meter above it, which the host moves along x.
SceneDesc range_scene() {
  NodeDesc target = node("Target", {0, 0, 0}, {1, 1, 1}, {});
  target.children = {3};
  NodeDesc box =
      node("Box", {0, 1, 0}, {1, 1, 1}, {{"body", std::string("kinematic")}, {"layer", std::string("hitbox")}});
  box.parent = 2;
  return scene_with({target, box});  // (the camera and the ground come first: the target is 2, its box 3)
}

// The host moves the target 10 m/s; a shot (the x a guest saw the target at) is judged both ways.
constexpr const char* kHostScript = R"(
  $target = Scene.find('Target')
  $shots = []
  def box_below?(x)
    hit = Physics.raycast(Vector3.new(x, 10, 0), Vector3.new(0, -1, 0), 20, layers: ['hitbox'])
    !hit.nil? && hit[:object].name == 'Box'
  end
  Network.on(:fire) do |x, from, view_time|
    rewound = Physics.rewind(view_time) { box_below?(x) }
    $shots << { from: from, view_time: view_time, behind: Network.time - view_time, rewound: rewound,
                now: box_below?(x) }
  end
)";

struct Range {
  SceneDesc scene;
  TempGame game;
  std::vector<std::unique_ptr<Engine>> machines;
  std::unique_ptr<Middleman> network;
  long rounds_played = 0;

  explicit Range(int max_rewind_ms) : scene(range_scene()), game(game_json(max_rewind_ms)) {
    Engine& host = add_machine();
    Engine& guest = add_machine();
    ruby(host, "Network.discovery_port = 47833");
    REQUIRE(ruby(host, "Network.host(port: 0, visible: false)") == "true");
    network = std::make_unique<Middleman>(static_cast<std::uint16_t>(std::stoi(ruby(host, "Network.port"))));
    REQUIRE(network->valid());
    network->delay(kOneWayRounds);
    REQUIRE(ruby(guest, "Network.join('127.0.0.1', port: " + std::to_string(network->port()) + ")") == "true");
    REQUIRE(until([&guest] { return guest.network().state() == net::NetworkSession::State::Online; }));
    for (auto& machine : machines) ruby(*machine, "Scene.find('Target').add_component(NetworkSync)");
    ruby(host, kHostScript);
  }

  Engine& host() { return *machines[0]; }
  Engine& guest() { return *machines[1]; }

  Engine& add_machine() {
    EngineOptions options;
    options.game_dir = game.dir;
    options.fixed_step = true;
    options.scene = &scene;
    options.network_time = [this] { return rounds_played * static_cast<double>(config::kDefaultFixedDeltaSeconds); };
    options.save_file = game.dir / ("save" + std::to_string(machines.size()) + ".json");
    auto created = Engine::create(options);
    if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
    machines.push_back(std::move(std::get<std::unique_ptr<Engine>>(created)));
    return *machines.back();
  }

  // A frame of both machines, the host's target a step further along first.
  void round() {
    ++rounds_played;
    ruby(host(), "$target.transform.position = Vector3.new(" + std::to_string(rounds_played * kStride) + ", 0, 0)");
    network->pump();
    for (auto& machine : machines) machine->frame(config::kDefaultFixedDeltaSeconds);
    std::vector<net::Watched> watched = network->watched();
    for (auto& machine : machines) {
      const std::vector<net::Watched> theirs = machine->network().watched();
      watched.insert(watched.end(), theirs.begin(), theirs.end());
    }
    net::Socket::wait(watched, test::kSocketPatience);
  }

  template <typename Done>
  bool until(Done done) {
    for (int i = 0; i < kMaxRounds; ++i) {
      round();
      if (done()) return true;
    }
    return false;
  }

  // The guest shoots kShots times at the target where it sees it; the host's judgements.
  std::string shots() {
    for (int i = 0; i < kSettleRounds; ++i) round();
    for (int shot = 0; shot < kShots; ++shot) {
      ruby(guest(), "Network.emit(:fire, Scene.find('Target').transform.position.x, to: :host)");
      for (int i = 0; i < kRoundsBetweenShots; ++i) round();
    }
    REQUIRE(until([this] { return ruby(host(), "$shots.size") == std::to_string(kShots); }));
    return ruby(host(), "$shots.map { |s| [s[:rewound], s[:now]] }.uniq.inspect");
  }

  static constexpr float kStride = 10.0f * config::kDefaultFixedDeltaSeconds;  // 10 m/s
};

}  // namespace

TEST_CASE("a shot judged where the guest saw its moving target hits it, and judged where it is now misses") {
  Range range(config::kDefaultMaxRewindMs);
  CHECK(range.shots() == "[[true, false]]");
  // The guest's view time came with every shot: well behind the host's time, within the cap.
  const float behind = number(range.host(), "$shots.map { |s| s[:behind] }.min");
  CHECK(behind > 0.1f);
  CHECK(number(range.host(), "$shots.map { |s| s[:behind] }.max") < 0.5f);
  CHECK(ruby(range.host(), "$shots.map { |s| s[:from] }.uniq.inspect") == "[2]");
  MESSAGE("PROBE: shots behind the host by "
          << ruby(range.host(), "$shots.map { |s| s[:behind].round(3) }.inspect")
          << " s; judged rewound and now: " << ruby(range.host(), "$shots.map { |s| [s[:rewound], s[:now]] }.inspect"));
}

TEST_CASE("a rewind goes no further back than game.json's network.max_rewind_ms, so a far shot misses") {
  Range range(kShortRewindMs);
  CHECK(range.shots() == "[[false, false]]");
}

TEST_CASE("Physics.rewind on the host's own time, or without a time, judges where things are now") {
  Range range(config::kDefaultMaxRewindMs);
  for (int i = 0; i < kSettleRounds; ++i) range.round();
  CHECK(ruby(range.host(), "Physics.rewind(nil) { box_below?($target.transform.position.x) }") == "true");
  CHECK(ruby(range.host(), "Physics.rewind(Network.time + 5) { box_below?($target.transform.position.x) }") == "true");
  CHECK(ruby(range.host(), "Physics.rewind(Network.time) { 7 }") == "7");
  CHECK(ruby(range.host(), "Physics.max_rewind") == "0.2");
  // A guest does not rewind: it judges nothing.
  CHECK(ruby(range.guest(), "Physics.rewind(0.0) { :ran }") == ":ran");
}
