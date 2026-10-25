#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <string>
#include <thread>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "net/network_session.hpp"
#include "integration/swordsman.hpp"
#include "net/rooms/relay_server.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr int kMaxRounds = 3000;  // of one frame on every machine, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr int kSettleRounds = 90;   // frames for a watched object to close in
constexpr float kBallStart = 3.0f;  // where the ball's template sits, along x
constexpr float kNear = 0.05f;      // meters: "the same place" across machines
constexpr std::size_t kManyTexts = 64;
constexpr float kFarAlong = 40.0f;            // where a hero is moved to, along x
constexpr int kAsksTooMany = 2;               // for what is spawned, beyond the answers a machine has saved up
constexpr int kRoundsForOneMoreAnswer = 150;  // frames: past the two seconds of game time that earn one
constexpr float kWhereTheGuestLeftIt = 2.9f;  // meters along x, give or take
constexpr int kQuarterSecondRounds = 15;      // frames
constexpr float kMostOfTheWayToFive = 4.5f;   // from 3, where the ball starts
constexpr float kShownBehind = 0.25f;         // seconds a watched swordsman is shown behind his owner
constexpr float kFewFrames = 4.0f * config::kFixedDeltaSeconds;  // what a pose takes on its way, and more
constexpr float kSomeMix = 0.05f;
constexpr int kPastAWholePose = 180;  // frames: three seconds, past the two between whole poses  // meters of the
                                      // sword's move that only a mixed clip makes

// A camera, a platform the host moves, a ball to spawn copies of, and a swordsman (swordsman.hpp).
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
  test::add_swordsman(desc);
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
  // The real time as the machines' networks read it, when not the clock's (set before they start).
  Stopwatch::Reading network_time;

  Lan() { std::filesystem::remove_all(dir); }
  ~Lan() {
    machines.clear();
    std::filesystem::remove_all(dir);
  }

  Engine& add_machine() {
    EngineOptions options;
    options.game_dir = fixture("games/sandbox");
    options.fixed_step = true;
    if (!level_from_files) options.scene = &scene;
    if (network_time) options.network_time = network_time;
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

// Where a machine's swordsman is in his own clip, and where his sword is.
float clip_time(Engine& engine) { return std::stof(ruby(engine, "$hero.animation.time")); }
float sword_x(Engine& engine) { return std::stof(ruby(engine, "$hero.children.first.transform.local_position.x")); }
std::string clip_of(Engine& engine) { return ruby(engine, "$hero.animation.clip.inspect"); }

// Every one of `machines` gives its swordsman a NetworkSync with these settings.
void sync_swordsman(std::initializer_list<Engine*> machines, const std::string& settings = "") {
  for (Engine* machine : machines) {
    ruby(*machine, "$hero = Scene.find('Swordsman'); $hero.add_component(NetworkSync" + settings + ")");
  }
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
  // (The host's scripts use these keys, which is what makes them symbols when they arrive.)
  ruby(*host, "$keys_in_use = %i[damage at tags crit note power]");
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

// A player hosting is held to what a dedicated server holds its players to.
TEST_CASE("a machine that floods a game a player hosts is let go") {
  Lan lan;
  lan.network_time = [] { return 0.0; };  // (as below: only the burst counts)
  auto [host, guest] = lan.host_and_guest();
  ruby(*guest, "64.times { Network.emit(:big, 'x' * 65_536) }");  // more bytes at once than any game sends
  REQUIRE(lan.until([guest = guest] { return guest->network().state() == net::NetworkSession::State::Offline; }));
  CHECK(ruby(*guest, "$log.inspect") == "[:connected, :disconnected]");
  REQUIRE(lan.until([host = host] { return ruby(*host, "Network.peers.inspect") == "[]"; }));
}

TEST_CASE("a machine that floods a game a player hosts with tiny messages is let go") {
  Lan lan;
  // Time stands still for the networks: what a slow machine takes to pass the messages on earns
  // the guest nothing back, so the burst alone is what it may send.
  lan.network_time = [] { return 0.0; };
  auto [host, guest] = lan.host_and_guest();
  // Few bytes, far more messages at once than poses of dozens of objects make.
  ruby(*host, "$got = 0; Network.on(:t) { $got += 1 }");
  ruby(*guest, "20_000.times { Network.emit(:t) }");
  REQUIRE(lan.until([guest = guest] { return guest->network().state() == net::NetworkSession::State::Offline; }));
  CHECK(ruby(*host, "Network.peers.inspect") == "[]");
  // Every message within the burst was heard (its hello took one); the one past it, which let the
  // guest go, was not.
  constexpr int kJoinMessages = 1;
  CHECK(ruby(*host, "$got") == std::to_string(static_cast<int>(net::kServedBurstMessages) - kJoinMessages));
}

// A host loading a level stands still while its guests go on: what they sent meanwhile arrives at
// once, and is no flood.
TEST_CASE("a guest that goes on at a game's pace while the host stands still is not let go") {
  constexpr int kStallFrames = 360;      // six seconds of the guest's frames while the host stands still
  constexpr int kAfterFrames = 120;      // and two more with both playing
  constexpr int kMessagesPerFrame = 60;  // 3600 a second: poses of dozens of objects
  Lan lan;
  double clock = 0.0;
  lan.network_time = [&clock] { return clock; };
  auto [host, guest] = lan.host_and_guest();
  ruby(*host, "$got = 0; Network.on(:t) { $got += 1 }");
  const std::string talk = std::to_string(kMessagesPerFrame) + ".times { Network.emit(:t) }";
  const auto frame = [&](bool host_plays) {
    clock += config::kFixedDeltaSeconds;
    ruby(*guest, talk);
    guest->frame(config::kFixedDeltaSeconds);
    if (host_plays) host->frame(config::kFixedDeltaSeconds);
  };
  for (int i = 0; i < kStallFrames; ++i) frame(false);
  // Both play on until the host heard all that was sent while it stood still (how fast that comes
  // is the machine's), and some more.
  const auto heard_the_stall = [&] { return std::stoi(ruby(*host, "$got")) >= kStallFrames * kMessagesPerFrame; };
  int dbg_drain = 0;
  std::fprintf(stderr, "DBG stall over\n");
  const auto dbg_t0 = std::chrono::steady_clock::now();
  const auto dbg_ms = [&] { return (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - dbg_t0).count(); };
  int dbg_last_got = -1;
  for (int i = 0; i < kMaxRounds && !heard_the_stall(); ++i) {
    frame(true), ++dbg_drain;
    const int got_now = std::stoi(ruby(*host, "$got"));
    if (got_now != dbg_last_got) { std::fprintf(stderr, "DBG frame %d ms %d got %d\n", i, dbg_ms(), got_now); dbg_last_got = got_now; }
    if (i == 100 || i == 400) { std::fprintf(stderr, "DBG ss at frame %d ms %d\n", i, dbg_ms()); std::fflush(stderr); (void)std::system("ss -tinm 'dst 127.0.0.1' 1>&2"); }
  }
  std::fprintf(stderr, "DBG drain ms %d\n", dbg_ms());
  std::fprintf(stderr, "DBG drain frames %d online %d got %s\n", dbg_drain, (int)(guest->network().state() == net::NetworkSession::State::Online), ruby(*host, "$got").c_str());
  for (int i = 0; i < kAfterFrames; ++i) frame(true);
  CHECK(heard_the_stall());
  CHECK(guest->network().state() == net::NetworkSession::State::Online);
}

TEST_CASE("a hash key no script here ever used arrives as a text, and makes no new symbol") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  ruby(*host, "$got = nil; Network.on(:hit) { |data, _from| $got = data; $damage = data[:damage] }");
  CHECK(ruby(*guest, "Network.emit(:hit, { damage: 3, 'key_only_the_sender_made_up' => 4 })") == "true");
  REQUIRE(lan.until([&] { return ruby(*host, "$got.nil?") == "false"; }));
  CHECK(ruby(*host, "$damage") == "3");
  CHECK(ruby(*host, "$got.keys.map { |key| key.class.to_s }.sort.inspect") == "[\"String\", \"Symbol\"]");
  CHECK(ruby(*host, "$got.keys.find { |key| key.is_a?(String) }") == "key_only_the_sender_made_up");
}

TEST_CASE("a message's name is shown in the log with nothing in it that fakes a line") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  ruby(*host, "Network.on(\"odd\\n[error] fake\") { raise 'broken handler' }");
  std::vector<std::string> lines;
  const log::Sink before = log::set_sink([&lines](log::Level, const std::string& line) { lines.push_back(line); });
  net::NetworkEvent event;
  event.peer = 2;
  event.name = "odd\n[error] fake";
  host->scripts().dispatch(event);
  log::set_sink(before);
  REQUIRE(lines.size() == 1);
  CHECK(lines.front().find("odd?[error] fake") != std::string::npos);
  CHECK(lines.front().find("odd\n") == std::string::npos);
}

namespace {

// An allocator that has nothing left to give (and still takes back what it is handed).
void* no_memory_left(mrb_state* mrb, void* block, std::size_t size, void* userdata) {
  if (size == 0) return mrb_default_allocf(mrb, block, size, userdata);
  return nullptr;
}

}  // namespace

TEST_CASE("no memory for what a message carries loses the message, not the game") {
  Lan lan;
  Engine& machine = lan.add_machine();
  mrb_state* mrb = machine.scripts().vm().state();
  net::NetworkEvent event;
  event.peer = 2;
  event.name = std::string(net::kMaxNameBytes, 'n');  // (too long to fit inside a Ruby string's own header)
  event.data = std::vector<std::string>(kManyTexts, std::string(net::kMaxNameBytes, 't'));
  const int errors_before = machine.scripts().error_count();
  const mrb_allocf real = mrb->allocf;
  mrb->allocf = no_memory_left;
  machine.scripts().dispatch(event);  // (outside the protected call, this ended the program)
  mrb->allocf = real;
  CHECK(machine.scripts().error_count() == errors_before + 1);
  CHECK(ruby(machine, "1 + 1") == "2");
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

TEST_CASE("a watched object is shown the delay it asks for behind its owner, or closes in at once without one") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  ruby(*host, "$platform = Scene.find('Platform'); $platform.add_component(NetworkSync)");
  ruby(*host, "$ball = Scene.find('Ball'); $ball.add_component(NetworkSync, key: 'ball')");
  ruby(*guest, "$platform = Scene.find('Platform'); $platform.add_component(NetworkSync, delay: 0.5)");
  ruby(*guest, "$ball = Scene.find('Ball'); $ball.add_component(NetworkSync, key: 'ball', interpolate: false)");
  lan.rounds(kSettleRounds);

  ruby(*host, "$platform.transform.position = Vector3.new(2, 0, 0); $ball.transform.position = Vector3.new(5, 0, 0)");
  lan.rounds(kQuarterSecondRounds);
  CHECK(x_of(*guest, "$platform") == doctest::Approx(0.0f).epsilon(kNear));  // half a second behind
  CHECK(x_of(*guest, "$ball") > kMostOfTheWayToFive);                        // on its way already
  lan.rounds(kSettleRounds);
  CHECK(x_of(*guest, "$platform") == doctest::Approx(2.0f).epsilon(kNear));
  CHECK(x_of(*guest, "$ball") == doctest::Approx(5.0f).epsilon(kNear));
}

TEST_CASE("an object handed back to a machine that watched it before is shown from what it hears now") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  for (Engine* machine : {host, guest}) {
    ruby(*machine, "$platform = Scene.find('Platform'); $sync = $platform.add_component(NetworkSync)");
  }
  lan.rounds(kSettleRounds);  // the guest watched the host's platform, at 0
  for (Engine* machine : {host, guest}) ruby(*machine, "$sync.assign(owner: 2)");
  ruby(*guest, "$platform.transform.position = Vector3.new(3, 0, 0)");
  lan.rounds(kSettleRounds);
  // The host takes it back, and moves it on from where the guest left it.
  for (Engine* machine : {host, guest}) ruby(*machine, "$sync.assign(owner: 1)");
  ruby(*host, "$platform.transform.position = Vector3.new(4, 0, 0)");
  float lowest = kFarAlong;
  for (int round = 0; round < kSettleRounds; ++round) {
    lan.round();
    lowest = std::min(lowest, x_of(*guest, "$platform"));
  }
  CHECK(lowest >= kWhereTheGuestLeftIt);  // never back where the host had it before
  CHECK(x_of(*guest, "$platform") == doctest::Approx(4.0f).epsilon(kNear));
}

TEST_CASE("a second NetworkSync under a key another object goes by is refused, and the first keeps it") {
  Lan lan;
  Engine& machine = lan.add_machine();
  const std::string holder = "Network.instance_variable_get(:@synced)['shared'].equal?($first)";
  // (Hosting: the objects are this machine's to send, frame after frame.)
  REQUIRE(ruby(machine, "Network.host(port: 0)") == "true");
  ruby(machine, "$first = Scene.find('Platform').add_component(NetworkSync, key: 'shared')");
  lan.round();
  const int errors_before = machine.scripts().error_count();
  ruby(machine, "$second = Scene.find('Ball').add_component(NetworkSync, key: 'shared')");
  lan.rounds(kSettleRounds);
  // Its start raised, and that is the only error: it is disabled, and never updated.
  CHECK(machine.scripts().error_count() == errors_before + 1);
  CHECK(ruby(machine, holder) == "true");
  // Handing an object over to a key that is taken raises, and changes nothing.
  ruby(machine, "$ball = Scene.find('Ball').add_component(NetworkSync, key: 'ball')");
  lan.round();
  CHECK(ruby(machine, "begin; $ball.assign(owner: 2, key: 'shared'); rescue ArgumentError => e; e.message; end")
            .find("is \"Platform\"'s already") != std::string::npos);
  CHECK(ruby(machine, "[$ball.key, $ball.owner].inspect") == "[\"ball\", 1]");
  CHECK(ruby(machine, holder) == "true");
  // Once the first object is gone, its key is free.
  ruby(machine, "Scene.find('Platform').destroy");
  lan.round();
  CHECK(ruby(machine, "$ball.assign(owner: 1, key: 'shared'); $ball.key") == "shared");
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

TEST_CASE("a game hosted directly lets as many machines from one address wait to join as it says") {
  Lan lan;
  Engine& machine = lan.add_machine();
  // (What scripts are told is the engine's own numbers.)
  CHECK(ruby(machine, "[Network::WAITING_PER_ADDRESS, Network::MOST_WAITING_PER_ADDRESS].inspect") ==
        "[" + std::to_string(net::kDefaultWaitingPerAddress) + ", " + std::to_string(net::kMaxWaitingLinks) + "]");
  CHECK(ruby(machine, "Network.host(port: 0, waiting_per_address: 32)") == "true");
  ruby(machine, "$ended = 0; Network.on_disconnect { $ended += 1 }");
  for (const char* wrong : {"0", "33"}) {
    CAPTURE(wrong);
    const std::string host = std::string("Network.host(port: 0, waiting_per_address: ") + wrong + ")";
    const RubyResult refused = machine.scripts().eval(host, "host.rb");
    CHECK_FALSE(refused.ok);
    CHECK(refused.error.find("waiting_per_address") != std::string::npos);
  }
  // A number it does not take is refused before anything else: the game hosted goes on.
  CHECK(ruby(machine, "[Network.host?, $ended].inspect") == "[true, 0]");
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
  ruby(*guest, "Network.__deliver(Network::SPAWN, ['Ball#2.1', 'Ball', 1, nil, nil], :all)");
  ruby(*guest, "Network.__deliver(Network::SPAWN, ['Platform#2.2', 'Platform', 2, nil, nil], :all)");
  ruby(*guest, "Network.__deliver(Network::SPAWNED, [['Ball#2.3', 'Ball', 2, nil, nil]], :all)");
  ruby(*guest, "Network.__deliver(Network::SPAWN, 'nonsense', :all)");
  ruby(*guest, "Network.__deliver(Network::SPAWN, ['Ball#2.4', 'Ball', 2, 'here', nil], :all)");
  ruby(*guest, "Network.__deliver(Network::SYNC, 'nonsense', :all)");
  // ... or a template the game does spawn, in a level that has none.
  ruby(*guest, "Network.__deliver(Network::SPAWN, ['Crate#2.6', 'Crate', 2, nil, nil], :all)");
  ruby(*guest, "Network.emit(:done)");
  ruby(*host, "$done = false; Network.on(:done) { $done = true }");
  REQUIRE(lan.until([&] { return ruby(*host, "$done") == "true"; }));
  CHECK(balls_on_host() == "1");
  CHECK(ruby(*host, "Scene.objects.count { |o| o.name == 'Platform' }") == "1");
  CHECK(ruby(*host, "Scene.find('Crate').nil?") == "true");
  CHECK(host->scripts().error_count() == 0);
  // What it may ask for: its own ball.
  ruby(*guest, "Network.__deliver(Network::SPAWN, ['Ball#2.5', 'Ball', 2, [1, 0, 0], nil], :all)");
  REQUIRE(lan.until([&] { return balls_on_host() == "2"; }));
}

namespace {

// Has `sender` say something to everyone and waits until `receiver` has heard all it said before.
void heard_out(Lan& lan, Engine& sender, Engine& receiver) {
  ruby(receiver, "$done = false; Network.on(:done) { $done = true }");
  ruby(sender, "Network.emit(:done)");
  REQUIRE(lan.until([&receiver] { return ruby(receiver, "$done") == "true"; }));
}

}  // namespace

TEST_CASE("a spawned object's key is its maker's to give: nobody takes the key something else goes by") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  const auto balls_on_host = [&] { return ruby(*host, "Scene.objects.count { |o| o.name == 'Ball' }"); };
  // The guest's hero, kept in the same place everywhere under a key of the game's choosing.
  for (Engine* machine : {host, guest}) {
    ruby(*machine, "$hero = Scene.find('Platform'); $hero.add_component(NetworkSync, owner: 2, key: 'hero-1')");
  }
  lan.round();
  // A guest asks for objects of its own under that key, under keys the host will give later,
  // under keys written as no machine writes them, and somewhere no object can be.
  for (const char* key :
       {"hero-1", "Ball#1.1", "Ball#1.2", "Ball#3.1", "Crate#2.1", "Ball#2.01", "Ball#2.-1", "Ball"}) {
    ruby(*guest, std::string("Network.__deliver(Network::SPAWN, ['") + key + "', 'Ball', 2, nil, nil], :all)");
  }
  ruby(*guest, "Network.__deliver(Network::SPAWN, ['Ball#2.1', 'Ball', 2, [1.0e12, 0, 0], nil], :all)");
  heard_out(lan, *guest, *host);
  lan.round();
  CHECK(balls_on_host() == "1");
  // The hero still is what goes by its key: where the guest moves it, the host sees it go.
  ruby(*guest, "$hero.transform.position = Vector3.new(" + std::to_string(kFarAlong) + ", 0, 0)");
  lan.rounds(kSettleRounds);
  CHECK(x_of(*host, "$hero") == doctest::Approx(kFarAlong).epsilon(kNear));
  // Its own key works, once: saying it again makes no second object.
  ruby(*guest, "2.times { Network.__deliver(Network::SPAWN, ['Ball#2.1', 'Ball', 2, nil, nil], :all) }");
  heard_out(lan, *guest, *host);
  CHECK(balls_on_host() == "2");
  // And the host's next ball gets the key the guest was after.
  ruby(*host, "$ball = Network.spawn('Ball')");
  CHECK(ruby(*host, "$ball.get_component(NetworkSync).key") == "Ball#1.1");
  CHECK(balls_on_host() == "3");
  CHECK(host->scripts().error_count() == 0);
}

namespace {

// The copies of the ball's template a machine has.
std::string balls_spawned_on(Engine& machine) {
  return ruby(machine, "Scene.objects.count { |o| o.name == 'Ball' } - 1");
}

}  // namespace

TEST_CASE("a machine has only so many spawned objects, by its own count as by everyone else's") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  const std::string most = ruby(*guest, "Network::MAX_SPAWNS_PER_PEER");
  ruby(*guest, "$made = Array.new(Network::MAX_SPAWNS_PER_PEER + 1) { Network.spawn('Ball') }");
  // One too many: nothing was made for it, here or on the host.
  CHECK(ruby(*guest, "[$made.compact.size, $made.last.nil?].inspect") == "[" + most + ", true]");
  heard_out(lan, *guest, *host);
  CHECK(balls_spawned_on(*guest) == most);
  CHECK(balls_spawned_on(*host) == most);
  // A guest that does not count is counted by the others.
  ruby(*guest, "Network.__deliver(Network::SPAWN, ['Ball#2.999', 'Ball', 2, nil, nil], :all)");
  heard_out(lan, *guest, *host);
  CHECK(balls_spawned_on(*host) == most);
  CHECK(host->scripts().error_count() + guest->scripts().error_count() == 0);
}

TEST_CASE("a game has only so many spawned objects, whoever they are for") {
  Lan lan;
  Engine& host = lan.add_machine();
  REQUIRE(ruby(host, "Network.host(port: 0)") == "true");
  ruby(host, R"(
    owners = 1..(Network::MAX_SPAWNS / Network::MAX_SPAWNS_PER_PEER)
    $made = owners.flat_map { |owner| Array.new(Network::MAX_SPAWNS_PER_PEER) { Network.spawn('Ball', owner: owner) } }
    $one_more = Network.spawn('Ball', owner: owners.last + 1)
  )");
  CHECK(ruby(host, "[$made.compact.size == Network::MAX_SPAWNS, $one_more.nil?].inspect") == "[true, true]");
  CHECK(balls_spawned_on(host) == ruby(host, "Network::MAX_SPAWNS"));
  CHECK(host.scripts().error_count() == 0);
}

TEST_CASE("a spawned object its owner destroys is gone on every machine, and its place is free again") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  Engine& third = lan.add_machine();
  lan.join(third, *host);
  ruby(*guest, "$mine = Network.spawn('Ball')");
  const auto balls_everywhere = [&](const std::string& count) {
    return balls_spawned_on(*host) == count && balls_spawned_on(third) == count;
  };
  REQUIRE(lan.until([&] { return balls_everywhere("1"); }));

  ruby(*guest, "$mine.destroy");
  CHECK(lan.until([&] { return balls_everywhere("0"); }));
  CHECK(balls_spawned_on(*guest) == "0");

  // A machine that only watches the object cannot take it from the others by destroying its copy.
  ruby(*guest, "$mine = Network.spawn('Ball')");
  REQUIRE(lan.until([&] { return balls_everywhere("1"); }));
  ruby(*host, "Scene.objects.select { |o| o.name == 'Ball' }.last.destroy");
  lan.rounds(kSettleRounds);
  CHECK(balls_spawned_on(third) == "1");
  CHECK(ruby(*guest, "$mine.alive?") == "true");
}

TEST_CASE("a spawned object destroyed before its NetworkSync started is forgotten all the same") {
  Lan lan;
  Engine& solo = lan.add_machine();
  // Spawned and destroyed inside one update: its NetworkSync would only have started on the next frame.
  ruby(solo, R"(
    class SpawnAndDrop < Behaviour
      def update(_dt) = @done ||= Network.spawn('Ball').destroy.nil?
    end
    Scene.find('Platform').add_component(SpawnAndDrop)
  )");
  lan.rounds(2);
  CHECK(ruby(solo, "Network.instance_variable_get(:@spawned).size") == "0");  // its place is free again
}

TEST_CASE("an object that goes by a spawned object's key does not take it along when it is destroyed") {
  Lan lan;
  Engine& host = lan.add_machine();
  REQUIRE(ruby(host, "Network.host(port: 0)") == "true");
  ruby(host, "$ball = Network.spawn('Ball'); $key = $ball.get_component(NetworkSync).key");
  ruby(host, "Scene.find('Platform').add_component(NetworkSync, key: $key)");
  lan.round();
  ruby(host, "Scene.find('Platform').destroy");
  lan.round();
  // The ball is still one of the game's spawned objects: whoever joins now is shown it.
  Engine& guest = lan.add_machine();
  lan.join(guest, host);
  REQUIRE(lan.until([&guest] { return balls_spawned_on(guest) == "1"; }));
}

TEST_CASE("a machine that keeps asking what is spawned is answered a few times, then only now and then") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  ruby(*host, "Network.spawn('Ball')");
  REQUIRE(lan.until([&] { return balls_spawned_on(*guest) == "1"; }));
  // The guest loses its copy and asks the host again: an answer brings the copy back.
  const auto answered = [&] {
    ruby(*guest, "Scene.objects.select { |o| o.name == 'Ball' }.drop(1).each(&:destroy)");
    lan.round();  // (the copy is gone with the frame)
    ruby(*guest, "Network.__deliver(Network::RESEND, nil, :host)");
    heard_out(lan, *guest, *host);
    heard_out(lan, *host, *guest);
    return balls_spawned_on(*guest) == "1";
  };
  const int saved_up = std::stoi(ruby(*host, "Network::Resends::SAVED_UP"));
  int answers = 0;
  for (int ask = 0; ask < saved_up + kAsksTooMany; ++ask) answers += answered() ? 1 : 0;
  CHECK(answers == saved_up);  // asking costs a few bytes and answering a whole list: not as often as asked
  lan.rounds(kRoundsForOneMoreAnswer);
  CHECK(answered());
  CHECK(host->scripts().error_count() == 0);
}

TEST_CASE("a host spawns only for machines in the game, and what it says in the wrong shape is ignored") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  const auto balls_on_guest = [&] { return ruby(*guest, "Scene.objects.count { |o| o.name == 'Ball' }"); };
  ruby(*guest, "$platform = Scene.find('Platform'); $platform.add_component(NetworkSync)");
  lan.round();
  // An object for a machine that is not there, and lists and places that are not what they should be.
  ruby(*host, "Network.__deliver(Network::SPAWN, ['Ball#1.1', 'Ball', 9, nil, nil], :all)");
  ruby(*host, "Network.__deliver(Network::SPAWNED, 'nonsense', :all)");
  ruby(*host, "Network.__deliver(Network::SPAWNED, [7, nil, ['Ball#1.2', 'Ball', 9, nil, nil]], :all)");
  ruby(*host, "Network.__deliver(Network::DESPAWN, [7, nil, ['Ball']], :all)");
  for (const char* placement : {"['Platform', 'here', nil]", "['Platform', [1, 2], [0, 0, 0]]", "['Platform']", "[]",
                                "[nil, nil, nil]", "['Platform', [1.0e12, 0, 0], [0, 0, 0]]"}) {
    ruby(*host, std::string("Network.__deliver(Network::SYNC, ") + placement + ", :all)");
  }
  heard_out(lan, *host, *guest);
  CHECK(balls_on_guest() == "1");
  CHECK(guest->scripts().error_count() == 0);  // (each of these once raised inside the handler)
  CHECK(x_of(*guest, "$platform") == doctest::Approx(0.0f));
  // For a machine that is in the game it works, under the host's own key for it.
  ruby(*host, "Network.__deliver(Network::SPAWN, ['Ball#1.3', 'Ball', 2, nil, nil], :all)");
  REQUIRE(lan.until([&] { return balls_on_guest() == "2"; }));
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

TEST_CASE("a character's own animation is seen on the other machines from its moment, as far behind as its pose") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  sync_swordsman({host, guest}, ", delay: 0.25");
  lan.rounds(kSettleRounds);
  ruby(*host, "$hero.animation.play(:Attack, loop: false)");
  REQUIRE(lan.until([&] { return clip_of(*guest) == "\"Attack\""; }));
  const float behind = clip_time(*host) - clip_time(*guest);
  CHECK(behind >= kShownBehind - kFewFrames);
  CHECK(behind <= kShownBehind + kFewFrames);
  lan.rounds(kSettleRounds);  // past the end of the attack
  CHECK(ruby(*guest, "$hero.animation.playing?") == "false");
  CHECK(sword_x(*guest) == doctest::Approx(test::kSwordReach));
  CHECK(sword_x(*host) == doctest::Approx(test::kSwordReach));
}

TEST_CASE("a machine that joins late sees what a character is doing already, its mixed clip too") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  sync_swordsman({host, guest});
  ruby(*host, "$hero.animation.play(:Guard); $hero.animation.mix(:Attack, weight: 0.5)");
  lan.rounds(kSettleRounds);
  Engine& late = lan.add_machine();
  lan.join(late, *host);
  sync_swordsman({&late});
  REQUIRE(lan.until([&] { return clip_of(late) == "\"Guard\""; }));
  // Guard alone leaves the sword at x = 0: what moves it along is the attack mixed over it.
  CHECK(lan.until([&] { return sword_x(late) > kSomeMix; }));
}

TEST_CASE("an object whose NetworkSync leaves animation out keeps its own clips") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  sync_swordsman({host});
  sync_swordsman({guest}, ", animation: false");
  ruby(*host, "$hero.animation.play(:Attack)");
  lan.rounds(kSettleRounds);
  CHECK(clip_of(*host) == "\"Attack\"");
  CHECK(clip_of(*guest) == "nil");
}

TEST_CASE("a character whose owner never animated it keeps the clips a watcher's scripts give it") {
  Lan lan;
  auto [host, guest] = lan.host_and_guest();
  sync_swordsman({host, guest});
  lan.rounds(kSettleRounds);
  ruby(*guest, "$hero.animation.play(:Guard)");  // say, a look of the guest's own
  lan.rounds(kPastAWholePose);                   // the host's whole poses come and go
  CHECK(clip_of(*guest) == "\"Guard\"");
  CHECK(clip_of(*host) == "nil");
}

TEST_CASE("an animation that arrives before its NetworkSync started is shown all the same") {
  Lan lan;
  Engine& machine = lan.add_machine();
  // (Not interpolated, and not started yet: what arrives is done at once.)
  ruby(machine, "$hero = Scene.find('Swordsman'); $sync = $hero.add_component(NetworkSync, interpolate: false)");
  ruby(machine, "$sync.__receive([0, 0, 0], [0, 0, 0], 1, [0.0, 0.0, ['Attack', 0.25, 1.0, true, true], nil])");
  CHECK(clip_of(machine) == "\"Attack\"");
}
