#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "fake_clock.hpp"
#include "integration/swordsman.hpp"
#include "net/socket/socket.hpp"
#include "net/traffic.hpp"
#include "server/game_server.hpp"
#include "server/server_program.hpp"
#include "server/stop_signals.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr int kMaxRounds = 3000;  // of one frame on every machine, a millisecond apart
constexpr std::chrono::milliseconds kRoundPause{1};
constexpr server::RoomLimits kOneRoomOfTwo{2, 1};
constexpr server::RoomLimits kTwoRoomsOfTwo{2, 2};
constexpr server::RoomLimits kRoomy{4, 2};
constexpr server::RoomLimits kTwoRoomsOfOne{1, 2};
constexpr int kTwoThreads = 2;
constexpr int kFramesToSayHello = 30;          // of a joining player's, for its hello to be on its way
constexpr int kVisitsPerRoom = 300;            // changes each room makes to one saved value
constexpr double kSlowLoadSeconds = 1.5;       // what a slow room's level takes to load (see kSlowScene)
constexpr int kSlowLevelBudgetMs = 10 * 1000;  // what its scripts may take, far more than that
// The longest a room may go without a frame while another loads: far less than the load.
constexpr double kLongestPause = kSlowLoadSeconds / 2;
constexpr double kLongestJoinSeconds = 10.0;
// The sandbox game's first level, which a server's room takes kSlowLoadSeconds to load once the
// server's save says "slow"; and what measures the longest pause between two frames of a room.
constexpr const char* kSlowScene = R"(
class PauseWatch < Behaviour
  def update(_dt)
    now = Time.now
    $longest_pause = [$longest_pause, now - @last].max if @last
    @last = now
  end
end

class MainScene < SceneScript
  def setup
    return unless Network.server? && Save["slow"]

    started = Time.now
    nil while Time.now - started < 1.5
  end
end
)";
constexpr int kServedTurns = 30;
constexpr int kOneThread = 1;
// A room's sleeps on the fake clock wake this late: its turns cover more than a tick each.
constexpr double kOversleep = config::kFixedDeltaSeconds;
// Game time comes in whole fixed steps: it trails the clock by less than one, give or take what
// adding up its steps in floats rounds off.
constexpr double kFloatRounding = 1e-3;
constexpr double kStepTolerance = config::kFixedDeltaSeconds + kFloatRounding;
constexpr double kClockTolerance = 1e-9;
constexpr std::chrono::milliseconds kHalfASecond{500};
constexpr double kPastMatchOverSeconds = 20.0;  // far past the seconds the platformer's server waits after a win
constexpr std::size_t kStatusReadBytes = 64 * 1024;
constexpr const char* kHeadEnd = "\r\n\r\n";
constexpr const char* kStatusGet = "GET /status HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  mrb_state* mrb = engine.scripts().vm().state();
  return RSTRING_PTR(mrb_obj_as_string(mrb, result.value));
}

// What `code` gives in the game of the server's room `room`, run on that room's own thread.
std::string in_room(server::GameServer& server, const std::string& room, const std::string& code) {
  RubyResult result;
  std::string text;
  const bool visited = server.visit(room, [&](Engine& game) {
    result = game.scripts().eval(code, "test.rb");
    if (result.ok) text = RSTRING_PTR(mrb_obj_as_string(game.scripts().vm().state(), result.value));
  });
  REQUIRE(visited);
  REQUIRE_MESSAGE(result.ok, result.error);
  return text;
}

// The sandbox game in a folder of its own, with a level that may load slowly (kSlowScene), and
// scripts given the time for it (past the engine's default budget).
struct SlowGame {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem-slow-level-test";
  std::filesystem::path game = root / "game";

  SlowGame() {
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy(fixture("games/sandbox"), game, std::filesystem::copy_options::recursive);
    std::ofstream(game / "scripts" / "main_scene.rb") << kSlowScene;
    std::ofstream(game / "game.json") << "{\"script_budget_ms\": " << kSlowLevelBudgetMs << "}";
  }
  ~SlowGame() { std::filesystem::remove_all(root); }
};

// The game time of a room and its clock's reading, both read at the same turn of the room's.
struct GameAndClock {
  double game = 0.0;
  double clock = 0.0;
};

bool online(Engine& engine) { return engine.network().state() == net::NetworkSession::State::Online; }
bool offline(Engine& engine) { return engine.network().state() == net::NetworkSession::State::Offline; }

// A dedicated server on this machine and the players who join it, each with an engine of its own.
struct Online {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-dedicated-server-test";
  server::GameServer server;
  std::vector<std::unique_ptr<Engine>> players;

  std::filesystem::path game;

  explicit Online(server::RoomLimits limits, std::filesystem::path played = fixture("games/sandbox"),
                  int threads = server::processor_count(), Clock* clock = nullptr)
      : server(settings(limits, played, threads, clock)), game(std::move(played)) {
    std::filesystem::remove_all(dir);
    std::string error;
    REQUIRE_MESSAGE(server.start(error), error);
  }
  ~Online() {
    players.clear();
    std::filesystem::remove_all(dir);
  }

  static server::ServerSettings settings(server::RoomLimits limits, const std::filesystem::path& played, int threads,
                                         Clock* clock) {
    server::ServerSettings settings;
    settings.clock = clock;
    settings.threads = threads;
    settings.game_dir = played;
    settings.port = 0;
    settings.limits = limits;
    settings.status.port = 0;  // (a free one: see status_port)
    return settings;
  }

  // A player of the game; with `scene`, of that scene instead of the game's own level.
  Engine& add_player(const SceneDesc* scene = nullptr) {
    EngineOptions options;
    options.game_dir = game;
    options.scene = scene;
    options.fixed_step = true;
    options.save_file = dir / ("save" + std::to_string(players.size()) + ".json");
    auto created = Engine::create(options);
    if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
    players.push_back(std::move(std::get<std::unique_ptr<Engine>>(created)));
    Engine& player = *players.back();
    ruby(player, "$log = []; Network.on_fail { $log << :failed }; Network.on_disconnect { $log << :disconnected }");
    return player;
  }

  void round() {
    server.tick(config::kFixedDeltaSeconds);
    for (auto& player : players) player->frame(config::kFixedDeltaSeconds);
    std::this_thread::sleep_for(kRoundPause);
  }

  bool until(const std::function<bool()>& done) {
    for (int i = 0; i < kMaxRounds; ++i) {
      round();
      if (done()) return true;
    }
    return false;
  }

  // Like until, with the players on the real clock as the rooms are (a frame of theirs a frame of
  // real time, not as fast as they go), for `seconds` at most.
  bool until_for(double seconds, const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
      round();
      if (done()) return true;
      std::this_thread::sleep_for(std::chrono::duration<double>(config::kFixedDeltaSeconds));
    }
    return false;
  }

  // Starts a new player joining: any room, or the one with `room` as its code.
  Engine& knock(const std::string& room = {}, const SceneDesc* scene = nullptr) {
    Engine& player = add_player(scene);
    const std::string asked = room.empty() ? "" : ", room: '" + room + "'";
    REQUIRE(ruby(player, "Network.join('127.0.0.1', port: " + std::to_string(server.port()) + asked + ")") == "true");
    return player;
  }

  // A new player in the game: any room, or the one with `room` as its code.
  Engine& join(const std::string& room = {}, const SceneDesc* scene = nullptr) {
    Engine& player = knock(room, scene);
    REQUIRE(until([&player] { return online(player); }));
    round();  // the others hear of it
    return player;
  }

  // What `code` gives in the game the server runs for the room `player` is in.
  std::string served(Engine& player, const std::string& code) { return in_room(server, player.network().room(), code); }

  // What the server's status port answers to `request` (a bare connection when it is empty), the
  // server's turns going on meanwhile, until it closes the connection.
  std::string ask_status(const std::string& request) {
    std::string error;
    net::Socket socket = net::Socket::connect_to(net::kThisMachine, server.status_port(), error);
    REQUIRE_MESSAGE(socket.valid(), error);
    std::vector<std::uint8_t> outgoing(request.begin(), request.end());
    std::vector<std::uint8_t> answer;
    bool connected = false;
    for (int i = 0; i < kMaxRounds; ++i) {
      round();
      if (!connected) connected = socket.connect_status() == net::IoStatus::Ok;
      if (!connected) continue;
      socket.write(outgoing);
      if (socket.read(answer, kStatusReadBytes) == net::IoStatus::Closed) break;
    }
    return std::string(answer.begin(), answer.end());
  }
};

// The JSON document of an HTTP answer.
nlohmann::json body_of(const std::string& answer) {
  const std::size_t head_end = answer.find(kHeadEnd);
  REQUIRE(head_end != std::string::npos);
  return nlohmann::json::parse(answer.substr(head_end + std::string(kHeadEnd).size()));
}

// What `player`'s room says Time.elapsed is, and what `clock` reads, at one of the room's turns.
GameAndClock game_and_clock(Online& online_game, Engine& player, const Clock& clock) {
  GameAndClock read;
  RubyResult result;
  std::string elapsed;
  const bool visited = online_game.server.visit(player.network().room(), [&](Engine& game) {
    result = game.scripts().eval("Time.elapsed", "test.rb");
    if (result.ok) elapsed = RSTRING_PTR(mrb_obj_as_string(game.scripts().vm().state(), result.value));
    read.clock = clock.now();
  });
  REQUIRE(visited);
  REQUIRE_MESSAGE(result.ok, result.error);
  read.game = std::stod(elapsed);
  return read;
}

}  // namespace

TEST_CASE("players join a dedicated server by its address, in one room, and the server plays nobody") {
  Online online_game(kRoomy);
  Engine& first = online_game.join();
  Engine& second = online_game.join();
  const std::string room = first.network().room();
  CHECK(room.size() == net::kRoomCodeLetters);
  CHECK(second.network().room() == room);
  CHECK(ruby(first, "[Network.host?, Network.server?, Network.id, Network.peers, Network.players].inspect") ==
        "[false, false, 2, [1, 3], [3]]");
  CHECK(ruby(second, "[Network.host?, Network.server?, Network.id, Network.peers, Network.players].inspect") ==
        "[false, false, 3, [1, 2], [2]]");
  CHECK(ruby(second, "Network.room") == room);

  CHECK(online_game.served(first, "[Network.online?, Network.host?, Network.server?, Network.id].inspect") ==
        "[true, true, true, 1]");
  CHECK(online_game.served(first, "[Network.peers.sort, Network.players.sort].inspect") == "[[2, 3], [2, 3]]");
  CHECK(online_game.server.room_count() == 1);
  CHECK(online_game.server.player_count() == 2);
}

TEST_CASE("messages pass through a dedicated server's room, whose game hears them too") {
  Online online_game(kRoomy);
  Engine& first = online_game.join();
  Engine& second = online_game.join();
  const std::string listen = "$got = []; Network.on(:hit) { |data, from| $got << [data, from] }";
  online_game.served(first, listen);
  ruby(second, listen);
  ruby(first, listen);
  CHECK(ruby(first, "Network.emit(:hit, 3)") == "true");
  CHECK(online_game.served(first, "Network.emit(:hit, 'from the server', to: 2)") == "true");
  REQUIRE(online_game.until([&] { return ruby(second, "$got.size") == "1" && ruby(first, "$got.size") == "1"; }));
  CHECK(ruby(second, "$got.inspect") == "[[3, 2]]");
  CHECK(online_game.served(first, "$got.inspect") == "[[3, 2]]");
  CHECK(ruby(first, "$got.inspect") == "[[\"from the server\", 1]]");
}

TEST_CASE("a full room sends the next player to a room of its own, and friends join a room by its code") {
  Online online_game(kTwoRoomsOfTwo);
  Engine& first = online_game.join();
  Engine& second = online_game.join();
  Engine& third = online_game.join();
  CHECK(second.network().room() == first.network().room());
  CHECK(third.network().room() != first.network().room());
  CHECK(third.network().id() == net::kFirstGuestId);
  CHECK(online_game.server.room_count() == 2);

  Engine& friend_of_third = online_game.join(third.network().room());
  CHECK(friend_of_third.network().room() == third.network().room());
  CHECK(ruby(third, "Network.players").find(std::to_string(friend_of_third.network().id())) != std::string::npos);

  // Nowhere left: not in a full room asked for by its code, nor anywhere by quick play, nor in a
  // room nobody has.
  Engine& too_late = online_game.knock(first.network().room());
  Engine& no_room_left = online_game.knock();
  Engine& wrong_code = online_game.knock("ZZZZZZ");
  REQUIRE(online_game.until([&] { return offline(too_late) && offline(no_room_left) && offline(wrong_code); }));
  for (Engine* refused : {&too_late, &no_room_left, &wrong_code}) CHECK(ruby(*refused, "$log.inspect") == "[:failed]");
  CHECK(online_game.server.player_count() == 4);
}

TEST_CASE("a match that ends closes its room: its players are told, and the next get a new game") {
  Online online_game(kOneRoomOfTwo);
  Engine& first = online_game.join();
  Engine& second = online_game.join();
  const std::string old_room = first.network().room();
  online_game.served(first, "$from_the_old_match = true; Game.quit");
  REQUIRE(online_game.until([&] { return offline(first) && offline(second); }));
  CHECK(online_game.server.room_count() == 0);
  REQUIRE(online_game.until([&] { return online_game.server.room_count() == 0; }));
  CHECK(ruby(first, "$log.inspect") == "[:disconnected]");
  CHECK(ruby(second, "$log.inspect") == "[:disconnected]");

  Engine& next = online_game.join();
  CHECK(next.network().room() != old_room);
  CHECK(online_game.served(next, "$from_the_old_match.inspect") == "nil");
}

TEST_CASE("a room whose players all left closes, and its place on the server is free again") {
  Online online_game(kOneRoomOfTwo);
  Engine& first = online_game.join();
  ruby(first, "Network.leave");
  REQUIRE(online_game.until([&] { return online_game.server.room_count() == 0; }));
  Engine& next = online_game.join();
  CHECK(online(next));
  CHECK(online_game.server.room_count() == 1);
}

TEST_CASE("a room that closed between two turns of the lobby holds no place: its server never counts too many") {
  test::LogCapture said;
  Online online_game(kOneRoomOfTwo);
  Engine& first = online_game.join();
  ruby(first, "Network.leave");
  // The next player's hello waits at the lobby while the old room closes, before the lobby's turn.
  Engine& next = online_game.knock();
  for (int frame = 0; frame < kFramesToSayHello; ++frame) {
    next.frame(config::kFixedDeltaSeconds);
    std::this_thread::sleep_for(kRoundPause);
  }
  std::this_thread::sleep_for(kHalfASecond);
  // (Its TLS handshake takes the lobby a few turns, all of them after the old room closed.)
  REQUIRE(online_game.until([&next] { return online(next); }));
  CHECK(online_game.server.room_count() == 1);
  online_game.server.stop();  // (its rooms' threads write to the log no more)
  CHECK(said.contains("opened (1 of 1)"));
  CHECK_FALSE(said.contains("(2 of 1)"));
}

TEST_CASE("on a dedicated server the menu never opens, the game never freezes, and saves stay in memory") {
  Online online_game(kRoomy);
  Engine& player = online_game.join();
  online_game.served(player, "$menu = Scene.objects.first.add_component(GameMenu)");
  // (The next visit comes a frame later at least: the menu started meanwhile.)
  CHECK(online_game.served(player, "$menu.show(:pause); $menu.finish('Over'); [$menu.open?, Time.scale].inspect") ==
        "[false, 1.0]");
  CHECK(online_game.served(player, "Save[:best] = 3; Save[:best] == 3") == "true");
}

TEST_CASE("a dedicated server's room plays the time its clock says passed, on its own thread") {
  // (Only the room's thread sleeps on it, really, so as not to spin: the clock still says how long.)
  test::FakeClock clock(kOversleep, test::RealSleep::Yes);
  Online online_game(kRoomy, fixture("games/sandbox"), kOneThread, &clock);
  Engine& player = online_game.join();
  const GameAndClock first = game_and_clock(online_game, player, clock);
  // Each visit comes at a later turn of the room's, each turn of which covered more than a tick.
  for (int turn = 0; turn < kServedTurns; ++turn) game_and_clock(online_game, player, clock);
  const GameAndClock last = game_and_clock(online_game, player, clock);
  const double clock_passed = last.clock - first.clock;
  CHECK(clock_passed > kServedTurns * config::kFixedDeltaSeconds);
  CHECK(std::abs(last.game - first.game - clock_passed) <= kStepTolerance);
}

TEST_CASE("the server's turns keep to its clock until it is asked to stop") {
  test::FakeClock clock;
  Online online_game(kRoomy, fixture("games/sandbox"), kOneThread, &clock);  // (no room: nothing else sleeps on it)
  const double started = clock.now();
  int turns = 0;
  bool on_time = true;
  server::serve_until(online_game.server, [&] {
    // Asked between turns: each came a tick after the one before, as the clock tells it.
    const double due = started + turns * config::kFixedDeltaSeconds;
    on_time = on_time && std::abs(clock.now() - due) <= kClockTolerance;
    return turns++ == kServedTurns;
  });
  CHECK(on_time);
  CHECK(clock.sleeps() == kServedTurns);
  CHECK(clock.slept() == doctest::Approx(kServedTurns * config::kFixedDeltaSeconds));
}

TEST_CASE("a player who floods a dedicated server's room is let go") {
  Online online_game(kRoomy);
  Engine& flooder = online_game.join();
  ruby(flooder, "64.times { Network.emit(:big, 'x' * 65_536) }");
  REQUIRE(online_game.until([&] { return offline(flooder); }));
  CHECK(ruby(flooder, "$log.inspect") == "[:disconnected]");
  REQUIRE(online_game.until([&] { return online_game.server.room_count() == 0; }));
}

TEST_CASE("a stopped server ends every match and tells its players") {
  Online online_game(kRoomy);
  Engine& first = online_game.join();
  online_game.server.stop();
  CHECK(online_game.server.room_count() == 0);
  REQUIRE(online_game.until([&] { return offline(first); }));
  CHECK(ruby(first, "$log.inspect") == "[:disconnected]");
}

TEST_CASE("a room loading a slow level holds up no other room: each runs on a thread of its own") {
  const SlowGame slow;
  Online online_game(kTwoRoomsOfOne, slow.game, kTwoThreads);
  Engine& first = online_game.join();
  online_game.served(first, "Save['slow'] = true; $longest_pause = 0.0; Scene.objects.first.add_component(PauseWatch)");
  online_game.served(first, "$longest_pause = 0.0");
  const auto started = std::chrono::steady_clock::now();
  // Into a room of its own, whose level loads slowly: on the real clock, the player waits for it
  // as long as a player would.
  Engine& second = online_game.knock();
  REQUIRE(online_game.until_for(kLongestJoinSeconds, [&second] { return online(second); }));
  const double waited = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  const double longest_pause = std::stod(online_game.served(first, "$longest_pause"));
  CHECK(second.network().room() != first.network().room());
  CHECK(online_game.server.thread_count() == kTwoThreads);
  CHECK(waited >= kSlowLoadSeconds);  // it did load slowly ...
  // ... and the first room played on meanwhile.
  CHECK(longest_pause < kLongestPause);
}

TEST_CASE("rooms changing one saved value at the same time lose none of each other's changes") {
  Online online_game(kTwoRoomsOfOne, fixture("games/sandbox"), kTwoThreads);
  const std::string first = online_game.join().network().room();
  const std::string second = online_game.join().network().room();
  REQUIRE(first != second);
  const std::string count_visits =
      std::to_string(kVisitsPerRoom) + ".times { Save.update('visits', 0) { |visits| visits + 1 } }";
  bool other_visited = false;
  std::thread other([&] {
    other_visited =
        online_game.server.visit(second, [&](Engine& room) { room.scripts().eval(count_visits, "test.rb"); });
  });
  in_room(online_game.server, first, count_visits);
  other.join();
  CHECK(other_visited);
  CHECK(in_room(online_game.server, first, "Save['visits']") == std::to_string(2 * kVisitsPerRoom));
}

TEST_CASE("SIGTERM stops a server whose rooms run on several threads: every match ends, every thread is joined") {
  Online online_game(kTwoRoomsOfOne, fixture("games/sandbox"), kTwoThreads);
  Engine& first = online_game.join();
  Engine& second = online_game.join();
  CHECK(online_game.server.thread_count() == kTwoThreads);
  const server::StopSignals signals;
  int turns = 0;
  server::serve_until(online_game.server, [&] {
    if (++turns == kServedTurns) std::raise(SIGTERM);
    return signals.asked();
  });
  online_game.server.stop();
  CHECK(online_game.server.thread_count() == 0);
  CHECK(online_game.server.room_count() == 0);
  REQUIRE(online_game.until([&] { return offline(first) && offline(second); }));
  for (Engine* player : {&first, &second}) CHECK(ruby(*player, "$log.inspect") == "[:disconnected]");
}

TEST_CASE("Ctrl+C and SIGTERM ask a server to stop instead of ending it") {
  for (const int signal : {SIGINT, SIGTERM}) {
    const server::StopSignals signals;
    CHECK_FALSE(signals.asked());
    std::raise(signal);
    CHECK(signals.asked());
  }
}

TEST_CASE("the platformer on a dedicated server plays no hero, keeps the coins, and ends the match after a win") {
  Online online_game(kRoomy, REDGEM_PLATFORMER_DIR);
  Engine& player = online_game.join();
  CHECK(ruby(player, "CoOp.instance.ghosts.size") == "0");  // none for the server
  // (The room hears of the player at its next frame, on its own thread.)
  CHECK(online_game.until([&] { return online_game.served(player, "CoOp.instance.ghosts.keys.inspect") == "[2]"; }));
  CHECK(online_game.served(player, "Scene.find('Player').get_component(Player).played?") == "false");

  ruby(player, "Network.emit(CoOp::COIN, 'Coin1')");
  REQUIRE(online_game.until([&] { return online_game.served(player, "GameManager.instance.coins") == "1"; }));
  ruby(player, "Network.emit(CoOp::WON)");
  REQUIRE(online_game.until([&] { return online_game.served(player, "GameManager.instance.won?") == "true"; }));
  CHECK(online_game.served(player, "[GameMenu.instance.open?, Time.scale].inspect") == "[false, 1.0]");
  CHECK(online_game.until_for(kPastMatchOverSeconds, [&] { return offline(player); }));
  REQUIRE(online_game.until([&] { return online_game.server.room_count() == 0; }));
}

TEST_CASE("a character's animation passes through a dedicated server's room to the other players") {
  SceneDesc arena;
  test::add_swordsman(arena);
  Online online_game(kRoomy);
  Engine& first = online_game.join({}, &arena);
  Engine& second = online_game.join({}, &arena);
  // The first player's swordsman (player 2), with the same key on both.
  const std::string sync = "$hero = Scene.find('Swordsman'); $hero.add_component(NetworkSync, owner: 2)";
  ruby(first, sync);
  ruby(second, sync);
  online_game.round();
  ruby(first, "$hero.animation.play(:Attack)");
  CHECK(online_game.until([&] { return ruby(second, "$hero.animation.clip.inspect") == "\"Attack\""; }));
}

TEST_CASE("a dedicated server's status port answers an HTTP GET with how busy the server is") {
  const net::TrafficTotals before = net::traffic_totals();
  Online online_game(kRoomy);
  REQUIRE(online_game.server.status_port() != 0);
  online_game.join();
  const std::string answer = online_game.ask_status(kStatusGet);
  CHECK(answer.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
  const nlohmann::json status = body_of(answer);
  CHECK_FALSE(status["engine_version"].get<std::string>().empty());
  CHECK(status["uptime_seconds"].get<double>() > 0.0);
  CHECK(status["rooms"]["open"] == 1);
  CHECK(status["rooms"]["limit"] == kRoomy.max_rooms);
  CHECK(status["rooms"]["list"][0]["players"] == 1);
  CHECK(status["players"]["total"] == 1);
  CHECK(status["players"]["limit_per_room"] == kRoomy.max_players);
  const nlohmann::json& frames = status["frame_ms"];
  CHECK(frames["worst"].get<double>() >= frames["average"].get<double>());
  CHECK(frames["budget"].get<double>() > 0.0);
  CHECK(status["bytes_per_second"]["in"].get<double>() >= 0.0);
  CHECK(status["threads"]["running"] == 1);
  // (Nothing of who plays, nor of the room's code.)
  CHECK(answer.find(online_game.players[0]->network().room()) == std::string::npos);
  // The joining player's bytes were counted on their way in and out.
  CHECK(net::traffic_totals().received > before.received);
  CHECK(net::traffic_totals().sent > before.sent);
}

TEST_CASE("a bare connection to a dedicated server's status port gets the document alone") {
  Online online_game(kRoomy);
  const std::string answer = online_game.ask_status("");
  REQUIRE_FALSE(answer.empty());
  CHECK(answer.back() == '\n');
  const nlohmann::json status = nlohmann::json::parse(answer);
  CHECK(status["rooms"]["open"] == 0);
  CHECK(status["players"]["total"] == 0);
}

TEST_CASE("a dedicated server's status port lets a request that says too much go unanswered") {
  Online online_game(kRoomy);
  CHECK(online_game.ask_status(std::string(server::kMostStatusRequestBytes * 2, 'x')).empty());
  // (And it answers the next one.)
  CHECK(online_game.ask_status(kStatusGet).rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
}

TEST_CASE("a dedicated server opens a status port only when told, and only on an address in numbers") {
  server::ServerSettings settings;
  settings.game_dir = fixture("games/sandbox");
  settings.port = 0;
  std::string error;
  {
    server::GameServer quiet(settings);
    REQUIRE_MESSAGE(quiet.start(error), error);
    CHECK(quiet.status_port() == 0);
  }
  settings.status.port = 0;
  settings.status.address = "localhost";
  server::GameServer misplaced(settings);
  CHECK_FALSE(misplaced.start(error));
  CHECK(error.find("status port") != std::string::npos);
}
