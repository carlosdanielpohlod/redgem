// Connection approval (Network.authenticate), with a player hosting, in a room on a relay and on
// a dedicated server: who is let in, who is turned away and why, and what the server keeps for
// each player it knows.

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "net/rooms/relay_server.hpp"
#include "server/game_server.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr int kMaxRounds = 3000;  // of one frame on every machine, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr int kWhileDeciding = 60;  // frames a newcomer is kept waiting on purpose
constexpr server::RoomLimits kRoomOfFour{4, 1};
constexpr int kPastJoinTimeoutRounds = 6 * 60;  // frames: longer than a join may keep quiet at a relay

// What the game hosting decides, by the token a newcomer carries: "good" is let in as
// "user-<name>", "bad" is turned away, and "later" waits for the test to decide ($later).
constexpr const char* kGatekeeper = R"(
$asked = []
$joined = []
Network.authenticate do |peer, auth, decide|
  $asked << [peer, auth]
  case auth[:token]
  when "good" then decide.accept(identity: "user-#{auth[:name]}", name: auth[:name])
  when "bad" then decide.deny("not logged in")
  when "later" then $later = decide
  end
end
Network.on_join { |peer| $joined << [peer, Network.identity(peer), Network.player_name(peer)] }
)";

// ... and on a dedicated server, what it keeps for each player when the player leaves.
constexpr const char* kServerScene = R"(
class MainScene < SceneScript
  def setup
    return unless Network.server?

    eval_gatekeeper
    Network.on_leave do |peer|
      players = Save["players"] || {}
      visits = (players[Network.identity(peer)] || {})["visits"] || 0
      players[Network.identity(peer)] = { "visits" => visits + 1, "name" => Network.player_name(peer) }
      Save["players"] = players
    end
  end
end
)";

// ... or with the same gatekeeper set up in a behaviour's start rather than in the scene's script.
constexpr const char* kGateBehaviour = R"(
class Gate < Behaviour
  def start
    eval_gatekeeper
  end
end
)";
constexpr const char* kSceneWithGate = R"(
class MainScene < SceneScript
  def setup
    Scene.find('Hero').add_component(Gate) if Network.server?
  end
end
)";
constexpr bool kGateInStart = true;

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

bool online(Engine& engine) { return engine.network().state() == net::NetworkSession::State::Online; }
bool offline(Engine& engine) { return engine.network().state() == net::NetworkSession::State::Offline; }

// The sandbox game in a folder of its own, whose server keeps players out unless they log in.
struct GuardedGame {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem-approval-test";
  std::filesystem::path game = root / "game";
  std::filesystem::path data = root / "data";

  explicit GuardedGame(bool gate_in_start = false) {
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy(fixture("games/sandbox"), game, std::filesystem::copy_options::recursive);
    if (gate_in_start) {
      std::ofstream(game / "scripts" / "gate.rb") << with_gatekeeper(kGateBehaviour);
      std::ofstream(game / "scripts" / "main_scene.rb") << kSceneWithGate;
    } else {
      std::ofstream(game / "scripts" / "main_scene.rb") << with_gatekeeper(kServerScene);
    }
  }

  static std::string with_gatekeeper(std::string script) {
    const std::string placeholder = "eval_gatekeeper";
    return script.replace(script.find(placeholder), placeholder.size(), kGatekeeper);
  }
  ~GuardedGame() { std::filesystem::remove_all(root); }
};

// Machines playing one game: a player hosting it, or a dedicated server's room; each machine
// with an engine of its own.
struct Gathering {
  explicit Gathering(bool gate_in_start = false) : files(gate_in_start) {}

  GuardedGame files;
  std::unique_ptr<server::GameServer> server;
  std::unique_ptr<net::RelayServer> relay;
  std::vector<std::unique_ptr<Engine>> machines;
  // The real time as the machines' networks read it: a frame's worth each round, so that a game's
  // time to decide runs out in rounds, as the server's and the relay's do.
  double clock = 0.0;

  Engine& add_machine() {
    EngineOptions options;
    options.game_dir = files.game;
    options.fixed_step = true;
    options.network_time = [this] { return clock; };
    options.save_file = files.root / ("save" + std::to_string(machines.size()) + ".json");
    auto created = Engine::create(options);
    if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
    machines.push_back(std::move(std::get<std::unique_ptr<Engine>>(created)));
    Engine& machine = *machines.back();
    ruby(machine, "$reasons = []; Network.on_fail { |reason| $reasons << reason }");
    return machine;
  }

  // A player hosting, deciding who comes in as kGatekeeper says.
  Engine& host() {
    Engine& host = add_machine();
    REQUIRE(ruby(host, "Network.host(port: 0, visible: false)") == "true");
    ruby(host, kGatekeeper);
    return host;
  }

  // A player who opened a room on a relay, deciding who comes in as kGatekeeper says.
  Engine& room_opener() {
    relay = std::make_unique<net::RelayServer>();
    std::string error;
    REQUIRE_MESSAGE(relay->start(0, error), error);
    Engine& opener = add_machine();
    REQUIRE(ruby(opener, relay_setting() + "; Network.host(room: true)") == "true");
    REQUIRE(until([&opener] { return online(opener); }));
    ruby(opener, kGatekeeper);
    return opener;
  }

  std::string relay_setting() const { return "Network.relay = '127.0.0.1:" + std::to_string(relay->port()) + "'"; }

  // Starts a new machine joining the room `code` on the relay with `auth` (Ruby).
  Engine& knock_room(const std::string& code, const std::string& auth) {
    Engine& guest = add_machine();
    REQUIRE(ruby(guest, relay_setting() + "; Network.join(room: '" + code + "', auth: " + auth + ")") == "true");
    return guest;
  }

  void start_server() {
    server::ServerSettings settings;
    settings.game_dir = files.game;
    settings.port = 0;
    settings.limits = kRoomOfFour;
    settings.data_dir = files.data;
    server = std::make_unique<server::GameServer>(settings);
    std::string error;
    REQUIRE_MESSAGE(server->start(error), error);
  }

  void stop_server() {
    server->stop();
    server.reset();
  }

  // Starts a new machine joining the game at `port` with `auth` (Ruby, or "nil").
  Engine& knock(std::uint16_t port, const std::string& auth) {
    Engine& guest = add_machine();
    REQUIRE(ruby(guest, "Network.join('127.0.0.1', port: " + std::to_string(port) + ", auth: " + auth + ")") == "true");
    return guest;
  }

  void round() {
    clock += config::kFixedDeltaSeconds;
    if (server) server->tick(config::kFixedDeltaSeconds);
    if (relay) relay->poll(config::kFixedDeltaSeconds);
    for (auto& machine : machines) machine->frame(config::kFixedDeltaSeconds);
    std::this_thread::sleep_for(kRoundPause);
  }

  bool until(const std::function<bool()>& done) {
    for (int i = 0; i < kMaxRounds; ++i) {
      round();
      if (done()) return true;
    }
    return false;
  }

  void rounds(int count) {
    for (int i = 0; i < count; ++i) round();
  }

  // What `code` gives in the game of the server's room `player` is in, run on the room's thread.
  std::string served(Engine& player, const std::string& code) {
    RubyResult result;
    std::string text;
    const bool visited = server->visit(player.network().room(), [&](Engine& room) {
      result = room.scripts().eval(code, "test.rb");
      if (result.ok) text = RSTRING_PTR(mrb_obj_as_string(room.scripts().vm().state(), result.value));
    });
    REQUIRE(visited);
    REQUIRE_MESSAGE(result.ok, result.error);
    return text;
  }

  nlohmann::json saved_players() const {
    std::string error;
    const auto saves = server::SaveDatabase::open(files.data / server::kSaveDatabaseName, error);
    REQUIRE_MESSAGE(saves != nullptr, error);
    return saves->get("players").value_or(nlohmann::json());
  }
};

std::uint16_t port_of(Engine& host) { return host.network().port(); }

}  // namespace

TEST_CASE("a player hosting lets in whom its game accepts, and knows who they are") {
  test::LogCapture quiet;
  Gathering game;
  Engine& host = game.host();
  Engine& guest = game.knock(port_of(host), "{ token: 'good', name: 'ana' }");
  REQUIRE(game.until([&guest] { return online(guest); }));
  game.round();
  CHECK(ruby(host, "$asked.inspect") == "[[2, {name: \"ana\", token: \"good\"}]]");
  CHECK(ruby(host, "$joined.inspect") == "[[2, \"user-ana\", \"ana\"]]");
  CHECK(ruby(host, "[Network.identity(2), Network.player_name(2)].inspect") == "[\"user-ana\", \"ana\"]");
  CHECK(ruby(guest, "[Network.id, Network.peers, Network.identity(2)].inspect") == "[2, [1], nil]");
}

TEST_CASE("on_join comes only once the game accepts, however long it takes to decide") {
  test::LogCapture quiet;
  Gathering game;
  Engine& host = game.host();
  Engine& guest = game.knock(port_of(host), "{ token: 'later' }");
  REQUIRE(game.until([&host] { return ruby(host, "$asked.size") == "1"; }));
  game.rounds(kWhileDeciding);
  CHECK(ruby(host, "[$joined, Network.peers].inspect") == "[[], []]");
  CHECK(ruby(guest, "Network.connecting?") == "true");
  CHECK(ruby(host, "$later.accept(identity: 'late-1')") == "true");
  CHECK(ruby(host, "$later.deny('changed my mind')") == "false");  // decided once
  REQUIRE(game.until([&guest] { return online(guest); }));
  game.round();
  CHECK(ruby(host, "$joined.inspect") == "[[2, \"late-1\", nil]]");
}

TEST_CASE("a player turned away hears why, and the game hosting never hears it joined") {
  test::LogCapture quiet;
  Gathering game;
  Engine& host = game.host();
  Engine& guest = game.knock(port_of(host), "{ token: 'bad' }");
  REQUIRE(game.until([&guest] { return offline(guest); }));
  game.rounds(kWhileDeciding);
  CHECK(ruby(guest, "$reasons.inspect") == "[\"not logged in\"]");
  CHECK(ruby(host, "[$joined, Network.peers].inspect") == "[[], []]");
}

TEST_CASE("a game that never decides turns the newcomer away when its time runs out") {
  test::LogCapture quiet;
  Gathering game;
  Engine& host = game.host();
  Engine& guest = game.knock(port_of(host), "{ token: 'never' }");
  REQUIRE(game.until([&guest] { return offline(guest); }));
  CHECK(ruby(guest, "$reasons.inspect") == "[\"the game did not decide in time\"]");
  CHECK(ruby(host, "$joined.inspect") == "[]");
  // ... and a decision that comes after that lets nobody in.
  CHECK(ruby(host, "Network::Decision.new(2, 1).accept(identity: 'x')") == "false");
}

TEST_CASE("a newcomer who leaves while the game decides is not let in by a late yes") {
  test::LogCapture quiet;
  Gathering game;
  Engine& host = game.host();
  Engine& guest = game.knock(port_of(host), "{ token: 'later' }");
  REQUIRE(game.until([&host] { return ruby(host, "$asked.size") == "1"; }));
  ruby(guest, "Network.leave");
  game.rounds(kWhileDeciding);
  CHECK(ruby(host, "$later.accept(identity: 'gone')") == "false");
  game.rounds(kWhileDeciding);
  CHECK(ruby(host, "[$joined, Network.peers, Network.identity(2)].inspect") == "[[], [], nil]");
}

TEST_CASE("with no handler everyone is let in, as before, whatever their join carries") {
  test::LogCapture quiet;
  Gathering game;
  Engine& host = game.add_machine();
  REQUIRE(ruby(host, "Network.host(port: 0, visible: false)") == "true");
  ruby(host, "$joined = []; Network.on_join { |peer| $joined << [peer, Network.identity(peer)] }");
  Engine& carrying = game.knock(port_of(host), "{ token: 'whatever' }");
  Engine& bare = game.knock(port_of(host), "nil");
  REQUIRE(game.until([&] { return online(carrying) && online(bare); }));
  game.round();
  CHECK(ruby(host, "$joined.sort.inspect") == "[[2, nil], [3, nil]]");
  // And a handler taken away lets everyone in again.
  ruby(host, kGatekeeper);
  ruby(host, "Network.authenticate");
  Engine& third = game.knock(port_of(host), "{ token: 'bad' }");
  CHECK(game.until([&third] { return online(third); }));
}

TEST_CASE("what a join carries is a small hash: anything else is refused before the game sees it") {
  test::LogCapture quiet;
  Gathering game;
  Engine& host = game.host();
  Engine& guest = game.add_machine();
  CHECK_FALSE(guest.scripts().eval("Network.join('127.0.0.1', port: 1, auth: 'token')", "t.rb").ok);
  CHECK_FALSE(guest.scripts().eval("Network.join('127.0.0.1', port: 1, auth: { t: 'x' * 5000 })", "t.rb").ok);
  // A program that is not this one sends them anyway.
  for (const nlohmann::json& carried : {nlohmann::json("token"), nlohmann::json{{"t", std::string(5000, 'x')}}}) {
    std::string error;
    REQUIRE(guest.network().join("127.0.0.1", port_of(host), error, carried));
    REQUIRE(game.until([&guest] { return offline(guest); }));
  }
  CHECK(ruby(guest, "$reasons.size") == "2");
  CHECK(ruby(guest, "$reasons.all? { |reason| reason.include?('auth:') }") == "true");
  CHECK(ruby(host, "$asked.inspect") == "[]");
}

TEST_CASE("in a room on a relay the game of whoever opened it decides who comes in, the relay nothing") {
  test::LogCapture quiet;
  Gathering game;
  Engine& opener = game.room_opener();
  const std::string code = opener.network().room();
  Engine& ana = game.knock_room(code, "{ token: 'good', name: 'ana' }");
  REQUIRE(game.until([&ana] { return online(ana); }));
  Engine& eve = game.knock_room(code, "{ token: 'bad' }");
  REQUIRE(game.until([&eve] { return offline(eve); }));
  game.round();
  CHECK(ruby(eve, "$reasons.inspect") == "[\"not logged in\"]");
  CHECK(ruby(opener, "$asked.inspect") == "[[2, {name: \"ana\", token: \"good\"}], [3, {token: \"bad\"}]]");
  CHECK(ruby(opener, "[$joined, Network.peers].inspect") == "[[[2, \"user-ana\", \"ana\"]], [2]]");
  CHECK(ruby(ana, "[Network.id, Network.peers, Network.room].inspect") == "[2, [1], \"" + code + "\"]");
}

TEST_CASE("a newcomer waits at the relay for as long as the room's game takes to decide, and no longer") {
  test::LogCapture quiet;
  Gathering game;
  Engine& opener = game.room_opener();
  Engine& late = game.knock_room(opener.network().room(), "{ token: 'later' }");
  REQUIRE(game.until([&opener] { return ruby(opener, "$asked.size") == "1"; }));
  game.rounds(kPastJoinTimeoutRounds);
  CHECK(ruby(late, "Network.connecting?") == "true");
  CHECK(ruby(opener, "$later.accept(identity: 'late-1')") == "true");
  REQUIRE(game.until([&late] { return online(late); }));
  game.round();
  CHECK(ruby(opener, "$joined.inspect") == "[[2, \"late-1\", nil]]");

  // One the game never decides on is turned away when its time runs out.
  Engine& forgotten = game.knock_room(opener.network().room(), "{ token: 'never' }");
  REQUIRE(game.until([&forgotten] { return offline(forgotten); }));
  CHECK(ruby(forgotten, "$reasons.inspect") == "[\"the game did not decide in time\"]");
  CHECK(ruby(opener, "Network.peers.inspect") == "[2]");
}

TEST_CASE("a newcomer who leaves a relay while the room's game decides is not let in by a late yes") {
  test::LogCapture quiet;
  Gathering game;
  Engine& opener = game.room_opener();
  Engine& guest = game.knock_room(opener.network().room(), "{ token: 'later' }");
  REQUIRE(game.until([&opener] { return ruby(opener, "$asked.size") == "1"; }));
  ruby(guest, "Network.leave");
  REQUIRE(game.until([&opener] { return opener.network().deciding() == 0; }));
  CHECK(ruby(opener, "$later.accept(identity: 'gone')") == "false");
  game.rounds(kWhileDeciding);
  CHECK(ruby(opener, "[$joined, Network.peers].inspect") == "[[], []]");
}

TEST_CASE("a room whose game stops deciding lets everyone in again, whatever their join carries") {
  test::LogCapture quiet;
  Gathering game;
  Engine& opener = game.room_opener();
  ruby(opener, "Network.authenticate");
  game.round();
  Engine& guest = game.knock_room(opener.network().room(), "{ token: 'bad' }");
  REQUIRE(game.until([&guest] { return online(guest); }));
  CHECK(ruby(opener, "$asked.inspect") == "[]");
}

TEST_CASE("a dedicated server's game decides who plays, and keeps each player's save across restarts") {
  test::LogCapture quiet;
  Gathering game;
  game.start_server();
  Engine& ana = game.knock(game.server->port(), "{ token: 'good', name: 'ana' }");
  Engine& eve = game.knock(game.server->port(), "{ token: 'bad', name: 'eve' }");
  REQUIRE(game.until([&] { return online(ana) && offline(eve); }));
  game.round();
  CHECK(ruby(eve, "$reasons.inspect") == "[\"not logged in\"]");
  // (The room hears of it at its next frame, on its own thread.)
  REQUIRE(game.until([&] { return game.served(ana, "$joined.size") == "1"; }));
  CHECK(game.served(ana, "$joined.inspect") == "[[2, \"user-ana\", \"ana\"]]");
  CHECK(game.served(ana, "Network.identity(2)") == "user-ana");

  ruby(ana, "Network.leave");
  REQUIRE(game.until([&game] { return game.server->room_count() == 0; }));
  CHECK(game.saved_players() == nlohmann::json{{"user-ana", {{"visits", 1}, {"name", "ana"}}}});
  CHECK(std::filesystem::exists(game.files.data / server::kSaveDatabaseName));

  // The server stops and starts again: what it kept is there for the player's next visit.
  game.stop_server();
  game.start_server();
  REQUIRE(ruby(ana, "Network.join('127.0.0.1', port: " + std::to_string(game.server->port()) +
                        ", auth: { token: 'good', name: 'ana' })") == "true");
  REQUIRE(game.until([&ana] { return online(ana); }));
  game.round();
  CHECK(game.served(ana, "Save['players']['user-ana']['visits']") == "1");
  ruby(ana, "Network.leave");
  REQUIRE(game.until([&game] { return game.server->room_count() == 0; }));
  CHECK(game.saved_players()["user-ana"]["visits"] == 2);
}

// A room's game starts its behaviours before it lets its first player in: a gatekeeper set up in
// a start is asked about that player too.
TEST_CASE("a dedicated server's room asks a gatekeeper set in a behaviour's start about its first player") {
  test::LogCapture quiet;
  Gathering game(kGateInStart);
  game.start_server();
  Engine& eve = game.knock(game.server->port(), "{ token: 'bad', name: 'eve' }");
  REQUIRE(game.until([&eve] { return offline(eve); }));
  game.round();
  CHECK(ruby(eve, "$reasons.inspect") == "[\"not logged in\"]");
}
