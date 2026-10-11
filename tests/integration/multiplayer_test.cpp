#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <thread>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "net/relay_server.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr int kMaxRounds = 3000;  // of one frame on every machine, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr int kSettleRounds = 90;   // frames for a watched object to close in
constexpr float kBallStart = 3.0f;  // where the ball's template sits, along x
constexpr float kNear = 0.05f;      // meters: "the same place" across machines

// A camera, a platform the host moves, and a ball to spawn copies of.
SceneDesc small_scene() {
  SceneDesc desc;
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  NodeDesc platform;
  platform.name = "Platform";
  NodeDesc ball;
  ball.name = "Ball";
  ball.local.position = {kBallStart, 0.0f, 0.0f};
  desc.nodes = {camera, platform, ball};
  return desc;
}

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  mrb_state* mrb = engine.scripts().vm().state();
  return RSTRING_PTR(mrb_obj_as_string(mrb, result.value));
}

// Machines running the same game, each with its own engine and save file.
struct Lan {
  SceneDesc scene = small_scene();
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-multiplayer-test";
  std::vector<std::unique_ptr<Engine>> machines;
  // With this, machines play the fixture game's own level from its files (which a restart loads
  // again) instead of the small scene above.
  bool level_from_files = false;
  // A relay the machines may meet on, kept going with them.
  std::unique_ptr<net::RelayServer> relay;

  Lan() { std::filesystem::remove_all(dir); }
  ~Lan() {
    machines.clear();
    std::filesystem::remove_all(dir);
  }

  Engine& add_machine() {
    EngineOptions options;
    options.game_dir = fixture("games/sandbox");
    options.headless = true;
    if (!level_from_files) options.scene = &scene;
    options.save_file = dir / ("save" + std::to_string(machines.size()) + ".json");
    auto created = Engine::create(options);
    if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
    machines.push_back(std::move(std::get<std::unique_ptr<Engine>>(created)));
    Engine& machine = *machines.back();
    // Every machine keeps a log of what its network told it.
    ruby(machine, R"(
      Network.allow_spawn('Ball', 'Crate')
      Network.discovery_port = 47832  # the tests' own: a game open on this machine does not answer on it
      $log = []
      Network.on_connect { $log << :connected }
      Network.on_fail { $log << :failed }
      Network.on_disconnect { $log << :disconnected }
      Network.on_join { |peer| $log << [:joined, peer] }
      Network.on_leave { |peer| $log << [:left, peer] }
    )");
    return machine;
  }

  void round() {
    if (relay) relay->poll(config::kFixedDeltaSeconds);
    for (auto& machine : machines) machine->frame(config::kFixedDeltaSeconds);
    std::this_thread::sleep_for(kRoundPause);
  }

  void rounds(int count) {
    for (int i = 0; i < count; ++i) round();
  }

  bool until(const std::function<bool()>& done) {
    for (int i = 0; i < kMaxRounds; ++i) {
      round();
      if (done()) return true;
    }
    return false;
  }

  // A host and a guest in the same game.
  std::pair<Engine*, Engine*> host_and_guest() {
    Engine& host = add_machine();
    Engine& guest = add_machine();
    REQUIRE(ruby(host, "Network.host(port: 0)") == "true");
    join(guest, host);
    return {&host, &guest};
  }

  void join(Engine& guest, Engine& host) {
    REQUIRE(ruby(guest, "Network.join('127.0.0.1', port: " + ruby(host, "Network.port") + ")") == "true");
    REQUIRE(until([&guest] { return guest.network().state() == net::NetworkSession::State::Online; }));
    round();  // the host hears of it
  }
};

float x_of(Engine& engine, const std::string& ruby_object) {
  return std::stof(ruby(engine, ruby_object + ".transform.position.x"));
}

}  // namespace

TEST_CASE("one machine hosts, another joins, and both scripts are told") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  CHECK(ruby(*host, "[Network.online?, Network.host?, Network.id, Network.peers].inspect") == "[true, true, 1, [2]]");
  CHECK(ruby(*guest, "[Network.online?, Network.host?, Network.id, Network.peers].inspect") == "[true, false, 2, [1]]");
  CHECK(ruby(*host, "$log.inspect") == "[:connected, [:joined, 2]]");
  CHECK(ruby(*guest, "$log.inspect") == "[:connected]");
  CHECK(ruby(*guest, "Network.port") == ruby(*host, "Network.port"));
}

TEST_CASE("scripts send each other named messages carrying Ruby values") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  ruby(*host, "$got = []; Network.on(:hit) { |data, from| $got << [data, from] }");
  ruby(*guest, "$got = []; Network.on(:hit) { |data, from| $got << [data, from] }");
  CHECK(ruby(*guest,
             "Network.emit(:hit, { damage: 3, at: Vector3.new(1, 2, 3), tags: [:fire, 'big'], crit: true, "
             "note: nil, power: 0.5 })") == "true");
  REQUIRE(lan.until([&] { return ruby(*host, "$got.size") == "1"; }));
  CHECK(ruby(*host,
             "data, from = $got.first; [from, data[:damage], data[:at].class, data[:at].y, data[:tags], "
             "data[:crit], data[:note], data[:power]].inspect") ==
        "[2, 3, Vector3, 2.0, [\"fire\", \"big\"], true, nil, 0.5]");

  // To one machine, by id or as :host; plain values travel too.
  CHECK(ruby(*host, "Network.emit(:hit, 'for you', to: 2)") == "true");
  CHECK(ruby(*guest, "Network.emit(:hit, 7, to: :host)") == "true");
  REQUIRE(lan.until([&] { return ruby(*host, "$got.size") == "2" && ruby(*guest, "$got.size") == "1"; }));
  CHECK(ruby(*guest, "$got.inspect") == "[[\"for you\", 1]]");
  CHECK(ruby(*host, "$got.last.inspect") == "[7, 2]");

  // What cannot travel is refused where it is sent, and nobody to send to is a plain false.
  CHECK_FALSE(host->scripts().eval("Network.emit(:hit, Scene.find('Ball'))", "bad.rb").ok);
  CHECK_FALSE(host->scripts().eval("Network.emit(:hit, Float::NAN)", "bad.rb").ok);
  CHECK(ruby(*host, "Network.emit(:hit, 1, to: 9)") == "false");
}

TEST_CASE("a handler that fails is reported and the game goes on") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  ruby(*host, "$seen = 0; Network.on(:boom) { raise 'broken handler' }; Network.on(:fine) { $seen += 1 }");
  const int errors_before = host->scripts().error_count();
  ruby(*guest, "Network.emit(:boom); Network.emit(:fine)");
  REQUIRE(lan.until([&] { return ruby(*host, "$seen") == "1"; }));
  CHECK(host->scripts().error_count() == errors_before + 1);
  CHECK(ruby(*host, "Network.online?") == "true");
}

TEST_CASE("an object with a NetworkSync is moved by its owner and follows on the other machines") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  // The level's own platform: the host's to move, by default.
  for (Engine* machine : {host, guest}) {
    ruby(*machine, "$platform = Scene.find('Platform'); $sync = $platform.add_component(NetworkSync)");
  }
  lan.round();
  CHECK(ruby(*host, "[$sync.mine?, $sync.owner, $sync.key].inspect") == "[true, 1, \"Platform\"]");
  CHECK(ruby(*guest, "$sync.mine?") == "false");

  ruby(*host,
       "$platform.transform.position = Vector3.new(2, 1, 0); $platform.transform.rotation = Vector3.new(0, 90, 0)");
  lan.rounds(kSettleRounds);
  CHECK(x_of(*guest, "$platform") == doctest::Approx(2.0f).epsilon(kNear));
  CHECK(std::stof(ruby(*guest, "$platform.transform.rotation.y")) == doctest::Approx(90.0f).epsilon(kNear));
  // Far behind, it jumps instead of sliding across the level.
  ruby(*host, "$platform.transform.position = Vector3.new(80, 1, 0)");
  REQUIRE(lan.until([&] { return x_of(*guest, "$platform") > 79.0f; }));
  // The watcher cannot move it: it is pulled back to where its owner has it.
  ruby(*guest, "$platform.transform.position = Vector3.new(78, 1, 0)");
  lan.rounds(kSettleRounds);
  CHECK(x_of(*guest, "$platform") == doctest::Approx(80.0f).epsilon(kNear));
  CHECK(x_of(*host, "$platform") == doctest::Approx(80.0f));
}

TEST_CASE("Network.spawn creates an object on every machine, for late joiners too, until its owner leaves") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  ruby(*guest, "$mine = Network.spawn('Ball', position: Vector3.new(5, 0, 0))");
  REQUIRE(lan.until([&] { return ruby(*host, "Scene.objects.count { |o| o.name == 'Ball' }") == "2"; }));
  ruby(*host,
       "$theirs = Scene.objects.select { |o| o.name == 'Ball' }.last; $sync = $theirs.get_component(NetworkSync)");
  lan.round();
  CHECK(ruby(*host, "[$sync.owner, $sync.mine?]") == "[2, false]");
  CHECK(ruby(*guest, "$mine.get_component(NetworkSync).mine?") == "true");
  CHECK(x_of(*host, "$theirs") == doctest::Approx(5.0f));

  ruby(*guest, "$mine.transform.position = Vector3.new(7, 0, 0)");
  lan.rounds(kSettleRounds);
  CHECK(x_of(*host, "$theirs") == doctest::Approx(7.0f).epsilon(kNear));

  // A third machine arrives later and finds the ball where it is now.
  Engine& late = lan.add_machine();
  lan.join(late, *host);
  REQUIRE(lan.until([&] { return ruby(late, "Scene.objects.count { |o| o.name == 'Ball' }") == "2"; }));
  ruby(late, "$theirs = Scene.objects.select { |o| o.name == 'Ball' }.last");
  CHECK(x_of(late, "$theirs") == doctest::Approx(7.0f).epsilon(kNear));
  CHECK(ruby(late, "Network.peers.sort.inspect") == "[1, 2]");
  CHECK(ruby(*guest, "$log.last.inspect") == "[:joined, 3]");

  // Its owner leaves: the ball goes with them, everywhere else.
  ruby(*guest, "Network.leave");
  REQUIRE(
      lan.until([&] { return ruby(*host, "$theirs.alive?") == "false" && ruby(late, "$theirs.alive?") == "false"; }));
  CHECK(ruby(*host, "$log.last.inspect") == "[:left, 2]");
  CHECK(ruby(late, "$log.last.inspect") == "[:left, 2]");
  // On the machine that left it is still there, and still its own.
  CHECK(ruby(*guest, "[$mine.alive?, $mine.get_component(NetworkSync).mine?, Network.online?, $log.last].inspect") ==
        "[true, true, false, :disconnected]");
}

TEST_CASE("when the host leaves the game is over for everyone, and what the others owned leaves them") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  ruby(*host, "$ball = Network.spawn('Ball')");
  REQUIRE(lan.until([&] { return ruby(*guest, "Scene.objects.count { |o| o.name == 'Ball' }") == "2"; }));
  ruby(*guest, "$theirs = Scene.objects.select { |o| o.name == 'Ball' }.last");
  ruby(*host, "Network.leave");
  REQUIRE(lan.until([&] { return ruby(*guest, "Network.online?") == "false"; }));
  lan.round();
  CHECK(ruby(*guest, "[$log.last, $theirs.alive?].inspect") == "[:disconnected, false]");
  CHECK(ruby(*guest, "Network.emit(:anyone)") == "false");
}

TEST_CASE("joining where nobody hosts fails after a moment, and a taken port cannot be hosted on") {
  Lan lan;
  Engine& host = lan.add_machine();
  Engine& other = lan.add_machine();
  REQUIRE(ruby(host, "Network.host(port: 0)") == "true");
  const std::string port = ruby(host, "Network.port");
  CHECK(ruby(other, "Network.host(port: " + port + ")") == "false");
  ruby(host, "Network.leave");
  ruby(other, "Network.join('127.0.0.1', port: " + port + ")");
  REQUIRE(lan.until([&] { return ruby(other, "Network.connecting?") == "false"; }));
  CHECK(ruby(other, "[Network.online?, $log.last].inspect") == "[false, :failed]");
  CHECK_FALSE(other.scripts().eval("Network.host(port: 70000)", "bad.rb").ok);
}

TEST_CASE("handlers end with the level; the connection does not") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  ruby(*host, "$got = 0; Network.on(:ping) { $got += 1 }; Game.load_scene('level2')");
  lan.round();
  ruby(*guest, "Network.emit(:ping)");
  lan.rounds(kSettleRounds);
  CHECK(ruby(*host, "[$got, Network.online?, Network.peers].inspect") == "[0, true, [2]]");
}

TEST_CASE("a machine only spawns for others what this game spawns, and only for the one asking") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  const auto balls_on_host = [&] { return ruby(*host, "Scene.objects.count { |o| o.name == 'Ball' }"); };
  // The engine's own message names are not for games to send ...
  CHECK_FALSE(guest->scripts().eval("Network.emit('$spawn', [])", "bad.rb").ok);
  // ... and a guest that sends them anyway gets nowhere: an object for someone else, a template
  // the game never spawns, a list only the host may send, something that is not a record at all.
  ruby(*guest, "Network.__deliver(Network::SPAWN, ['k1', 'Ball', 1, nil, nil], :all)");
  ruby(*guest, "Network.__deliver(Network::SPAWN, ['k2', 'Platform', 2, nil, nil], :all)");
  ruby(*guest, "Network.__deliver(Network::SPAWNED, [['k3', 'Ball', 2, nil, nil]], :all)");
  ruby(*guest, "Network.__deliver(Network::SPAWN, 'nonsense', :all)");
  ruby(*guest, "Network.__deliver(Network::SPAWN, ['k4', 'Ball', 2, 'here', nil], :all)");
  ruby(*guest, "Network.__deliver(Network::SYNC, 'nonsense', :all)");
  ruby(*guest, "Network.emit(:done)");
  ruby(*host, "$done = false; Network.on(:done) { $done = true }");
  REQUIRE(lan.until([&] { return ruby(*host, "$done") == "true"; }));
  CHECK(balls_on_host() == "1");
  CHECK(ruby(*host, "Scene.objects.count { |o| o.name == 'Platform' }") == "1");
  CHECK(host->scripts().error_count() == 0);
  // What it may ask for: its own ball.
  ruby(*guest, "Network.__deliver(Network::SPAWN, ['k5', 'Ball', 2, [1, 0, 0], nil], :all)");
  REQUIRE(lan.until([&] { return balls_on_host() == "2"; }));
}

TEST_CASE("offline, everything is this machine's; what it spawned then joins the game with it") {
  Lan lan;
  Engine& host = lan.add_machine();
  Engine& guest = lan.add_machine();
  // The level's own platform belongs to the host: with no game on, that is whoever is playing.
  ruby(guest, "$sync = Scene.find('Platform').add_component(NetworkSync); $ball = Network.spawn('Ball')");
  lan.round();
  CHECK(ruby(guest, "[$sync.mine?, $ball.get_component(NetworkSync).mine?, Network.id].inspect") == "[true, true, 0]");

  REQUIRE(ruby(host, "Network.host(port: 0)") == "true");
  lan.join(guest, host);
  CHECK(ruby(guest, "[$sync.mine?, $ball.get_component(NetworkSync).owner].inspect") == "[false, 2]");
  REQUIRE(lan.until([&] { return ruby(host, "Scene.objects.count { |o| o.name == 'Ball' }") == "2"; }));
  ruby(host, "$theirs = Scene.objects.select { |o| o.name == 'Ball' }.last");
  lan.round();
  CHECK(ruby(host, "$theirs.get_component(NetworkSync).owner") == "2");

  // Out of the game again it is the guest's alone, and can go into the next game.
  ruby(guest, "Network.leave");
  CHECK(ruby(guest, "[$ball.alive?, $ball.get_component(NetworkSync).owner, $sync.mine?].inspect") ==
        "[true, 0, true]");
  REQUIRE(lan.until([&] { return ruby(host, "$theirs.alive?") == "false"; }));
}

TEST_CASE("hosting or joining again leaves the game this machine was in, and the others hear of it") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  REQUIRE(ruby(*host, "Network.host(port: 0)") == "true");
  CHECK(ruby(*host, "[$log.count(:disconnected), Network.peers].inspect") == "[1, []]");
  REQUIRE(lan.until([&] { return ruby(*guest, "Network.online?") == "false"; }));
  CHECK(ruby(*guest, "$log.last.inspect") == ":disconnected");
}

TEST_CASE("after a level is loaded, the objects of the others are back and one's own are gone elsewhere") {
  Lan lan;
  lan.level_from_files = true;
  auto [host, guest] = lan.host_and_guest();
  const auto crates = [](Engine& machine) { return ruby(machine, "Scene.objects.count { |o| o.name == 'Crate' }"); };
  REQUIRE(crates(*host) == "1");  // the level's own
  ruby(*host, "Network.spawn('Crate', position: Vector3.new(4, 9, 0))");
  ruby(*guest, "Network.spawn('Crate', position: Vector3.new(9, 9, 0))");
  REQUIRE(lan.until([&] { return crates(*host) == "3" && crates(*guest) == "3"; }));

  // The guest restarts the level: its own crate went with the old level, everywhere; the host's
  // is shown to it again.
  ruby(*guest, "Game.load_scene('main')");
  REQUIRE(lan.until([&] { return crates(*host) == "2" && crates(*guest) == "2"; }));
  CHECK(ruby(*guest, "Network.online?") == "true");
}

TEST_CASE("machines that cannot reach each other meet in a room on a relay, by its code") {
  Lan lan;
  lan.relay = std::make_unique<net::RelayServer>();
  std::string error;
  REQUIRE_MESSAGE(lan.relay->start(0, error), error);
  Engine& opener = lan.add_machine();
  Engine& guest = lan.add_machine();
  CHECK_FALSE(opener.scripts().eval("Network.host(room: true)", "no_relay.rb").ok);  // where is the relay?
  for (Engine* machine : {&opener, &guest}) {
    ruby(*machine, "Network.relay = '127.0.0.1:" + std::to_string(lan.relay->port()) + "'");
  }
  REQUIRE(ruby(opener, "Network.host(room: true)") == "true");
  REQUIRE(lan.until([&] { return ruby(opener, "Network.online?") == "true"; }));
  const std::string code = ruby(opener, "Network.room");
  CHECK(code.size() == net::kRoomCodeLetters);
  CHECK(ruby(opener, "[Network.host?, Network.id, $log].inspect") == "[true, 1, [:connected]]");

  // The code is typed by a person: small letters and stray spaces are forgiven.
  REQUIRE(ruby(guest, "Network.join(room: ' " + code + " '.downcase)") == "true");
  REQUIRE(lan.until([&] { return ruby(guest, "Network.online?") == "true"; }));
  lan.round();
  CHECK(ruby(guest, "[Network.host?, Network.id, Network.peers, Network.room].inspect") ==
        "[false, 2, [1], \"" + code + "\"]");
  CHECK(ruby(opener, "$log.last.inspect") == "[:joined, 2]");

  // Everything a game does over the network works the same in a room.
  ruby(opener, "$got = nil; Network.on(:hi) { |data, from| $got = [data, from] }");
  ruby(guest, "Network.emit(:hi, { n: 1 }); $ball = Network.spawn('Ball', position: Vector3.new(5, 0, 0))");
  REQUIRE(lan.until([&] { return ruby(opener, "Scene.objects.count { |o| o.name == 'Ball' }") == "2"; }));
  CHECK(ruby(opener, "$got.inspect") == "[{n: 1}, 2]");

  ruby(guest, "Network.join(room: 'ZZZZ')");  // no such room
  REQUIRE(lan.until([&] { return ruby(guest, "Network.connecting?") == "false"; }));
  CHECK(ruby(guest, "[Network.online?, $log.last].inspect") == "[false, :failed]");
}

TEST_CASE("a hosted game is found by the machines around it, by name, unless it hides") {
  Lan lan;
  Engine& host = lan.add_machine();
  Engine& seeker = lan.add_machine();
  REQUIRE(ruby(host, "Network.host(port: 0, name: 'Find me if you can')") == "true");
  const std::string port = ruby(host, "Network.port");
  const std::string find = "Network.games.find { |game| game[:port] == " + port + " }";
  CHECK(ruby(seeker, "Network.games.inspect") == "[]");
  REQUIRE(ruby(seeker, "Network.search") == "true");
  REQUIRE(lan.until([&] { return ruby(seeker, find + ".nil?") == "false"; }));
  CHECK(ruby(seeker, "game = " + find + "; [game[:name], game[:players], game[:address].empty?].inspect") ==
        "[\"Find me if you can\", 1, false]");
  // Joining what was found needs nothing typed.
  REQUIRE(ruby(seeker, "game = " + find + "; Network.join(game[:address], port: game[:port])") == "true");
  REQUIRE(lan.until([&] { return ruby(seeker, "Network.online?") == "true"; }));

  // A game that asked not to be seen is not.
  ruby(seeker, "Network.leave; Network.stop_search");
  REQUIRE(ruby(host, "Network.host(port: 0, visible: false)") == "true");
  const std::string hidden_port = ruby(host, "Network.port");
  ruby(seeker, "Network.search");
  lan.rounds(kSettleRounds * 2);
  CHECK(ruby(seeker, "Network.games.inspect") == "[]");  // neither the hidden game nor the one hosted before it
}
