#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <functional>
#include <thread>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
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
constexpr float kTenthOfASecond = 0.1f;
constexpr int kServedTurns = 30;
constexpr double kLateness = 0.9;  // a turn may come a little early on a busy clock, never much
constexpr float kRealTimeTolerance = 0.1f;
constexpr int kPastMatchOverRounds = 6 * 60;  // frames: past the seconds the platformer's server waits after a win

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  mrb_state* mrb = engine.scripts().vm().state();
  return RSTRING_PTR(mrb_obj_as_string(mrb, result.value));
}

bool online(Engine& engine) { return engine.network().state() == net::NetworkSession::State::Online; }
bool offline(Engine& engine) { return engine.network().state() == net::NetworkSession::State::Offline; }

// A dedicated server on this machine and the players who join it, each with an engine of its own.
struct Online {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-dedicated-server-test";
  server::GameServer server;
  std::vector<std::unique_ptr<Engine>> players;

  std::filesystem::path game;

  explicit Online(server::RoomLimits limits, std::filesystem::path played = fixture("games/sandbox"))
      : server(settings(limits, played)), game(std::move(played)) {
    std::filesystem::remove_all(dir);
    std::string error;
    REQUIRE_MESSAGE(server.start(error), error);
  }
  ~Online() {
    players.clear();
    std::filesystem::remove_all(dir);
  }

  static server::ServerSettings settings(server::RoomLimits limits, const std::filesystem::path& played) {
    server::ServerSettings settings;
    settings.game_dir = played;
    settings.port = 0;
    settings.limits = limits;
    return settings;
  }

  Engine& add_player() {
    EngineOptions options;
    options.game_dir = game;
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

  // Starts a new player joining: any room, or the one with `room` as its code.
  Engine& knock(const std::string& room = {}) {
    Engine& player = add_player();
    const std::string asked = room.empty() ? "" : ", room: '" + room + "'";
    REQUIRE(ruby(player, "Network.join('127.0.0.1', port: " + std::to_string(server.port()) + asked + ")") == "true");
    return player;
  }

  // A new player in the game: any room, or the one with `room` as its code.
  Engine& join(const std::string& room = {}) {
    Engine& player = knock(room);
    REQUIRE(until([&player] { return online(player); }));
    round();  // the others hear of it
    return player;
  }

  // The game the server runs for the room `player` is in.
  Engine& room_of(Engine& player) {
    Engine* room = server.room(player.network().room());
    REQUIRE(room != nullptr);
    return *room;
  }
};

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

  Engine& served = online_game.room_of(first);
  CHECK(ruby(served, "[Network.online?, Network.host?, Network.server?, Network.id].inspect") ==
        "[true, true, true, 1]");
  CHECK(ruby(served, "[Network.peers.sort, Network.players.sort].inspect") == "[[2, 3], [2, 3]]");
  CHECK(online_game.server.room_count() == 1);
  CHECK(online_game.server.player_count() == 2);
}

TEST_CASE("messages pass through a dedicated server's room, whose game hears them too") {
  Online online_game(kRoomy);
  Engine& first = online_game.join();
  Engine& second = online_game.join();
  Engine& served = online_game.room_of(first);
  for (Engine* machine : {&second, &served})
    ruby(*machine, "$got = []; Network.on(:hit) { |data, from| $got << [data, from] }");
  ruby(first, "$got = []; Network.on(:hit) { |data, from| $got << [data, from] }");
  CHECK(ruby(first, "Network.emit(:hit, 3)") == "true");
  CHECK(ruby(served, "Network.emit(:hit, 'from the server', to: 2)") == "true");
  REQUIRE(online_game.until([&] { return ruby(second, "$got.size") == "1" && ruby(first, "$got.size") == "1"; }));
  CHECK(ruby(second, "$got.inspect") == "[[3, 2]]");
  CHECK(ruby(served, "$got.inspect") == "[[3, 2]]");
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
  ruby(online_game.room_of(first), "$from_the_old_match = true; Game.quit");
  REQUIRE(online_game.until([&] { return offline(first) && offline(second); }));
  CHECK(online_game.server.room_count() == 0);
  CHECK(ruby(first, "$log.inspect") == "[:disconnected]");
  CHECK(ruby(second, "$log.inspect") == "[:disconnected]");

  Engine& next = online_game.join();
  CHECK(next.network().room() != old_room);
  CHECK(ruby(online_game.room_of(next), "$from_the_old_match.inspect") == "nil");
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

TEST_CASE("on a dedicated server the menu never opens, the game never freezes, and saves stay in memory") {
  Online online_game(kRoomy);
  Engine& served = online_game.room_of(online_game.join());
  ruby(served, "$menu = Scene.objects.first.add_component(GameMenu)");
  online_game.round();
  CHECK(ruby(served, "$menu.show(:pause); $menu.finish('Over'); [$menu.open?, Time.scale].inspect") == "[false, 1.0]");
  CHECK(ruby(served, "Save[:best] = 3; Save[:best] == 3") == "true");
}

TEST_CASE("a dedicated server's room plays the time that really passed, not one step a turn") {
  Online online_game(kRoomy);
  Engine& served = online_game.room_of(online_game.join());
  const float before = std::stof(ruby(served, "Time.elapsed"));
  online_game.server.tick(kTenthOfASecond);
  const float after = std::stof(ruby(served, "Time.elapsed"));
  // (Within the one fixed step the engine may keep for the next turn.)
  CHECK(std::abs(after - before - kTenthOfASecond) <= config::kFixedDeltaSeconds);
}

TEST_CASE("the server's turns keep to the real clock until it is asked to stop") {
  Online online_game(kRoomy);
  Engine& served = online_game.room_of(online_game.join());
  const float game_before = std::stof(ruby(served, "Time.elapsed"));
  int turns = 0;
  const auto started = std::chrono::steady_clock::now();
  server::serve_until(online_game.server, [&turns] { return turns++ == kServedTurns; });
  const double real = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  CHECK(real >= kServedTurns * config::kFixedDeltaSeconds * kLateness);
  const float game = std::stof(ruby(served, "Time.elapsed")) - game_before;
  CHECK(std::abs(game - static_cast<float>(real)) <= kRealTimeTolerance);
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
  Engine& served = online_game.room_of(player);
  CHECK(ruby(player, "CoOp.instance.ghosts.size") == "0");  // none for the server
  CHECK(ruby(served, "CoOp.instance.ghosts.keys.inspect") == "[2]");
  CHECK(ruby(served, "Scene.find('Player').get_component(Player).played?") == "false");

  ruby(player, "Network.emit(CoOp::COIN, 'Coin1')");
  REQUIRE(online_game.until([&] { return ruby(served, "GameManager.instance.coins") == "1"; }));
  ruby(player, "Network.emit(CoOp::WON)");
  REQUIRE(online_game.until([&] { return ruby(served, "GameManager.instance.won?") == "true"; }));
  CHECK(ruby(served, "[GameMenu.instance.open?, Time.scale].inspect") == "[false, 1.0]");
  for (int round = 0; round < kPastMatchOverRounds && online(player); ++round) online_game.round();
  CHECK(offline(player));
  CHECK(online_game.server.room_count() == 0);
}
