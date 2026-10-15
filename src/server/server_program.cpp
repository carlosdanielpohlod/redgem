#include "server/server_program.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include "core/config.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "server/server_command.hpp"
#include "server/stop_signals.hpp"
#include "server/tick_pacer.hpp"

namespace redgem::server {

namespace {

// Seconds on a clock that only goes forward.
double seconds_now() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// How many rooms and players a server has, said in the log when it changes.
struct Census {
  std::size_t rooms = 0;
  std::size_t players = 0;

  bool same_as(const GameServer& server) const {
    return server.room_count() == rooms && server.player_count() == players;
  }

  void report(const GameServer& server) {
    if (same_as(server)) return;
    rooms = server.room_count();
    players = server.player_count();
    log::info(std::to_string(rooms) + " room(s) open, " + std::to_string(players) + " player(s)");
  }
};

int run_server(const ServerSettings& settings) {
  GameServer server(settings);
  std::string error;
  if (!server.start(error)) {
    log::error("cannot serve " + settings.game_dir.string() + ": " + error);
    return kExitFailure;
  }
  const StopSignals signals;
  log::info("game server listening on port " + std::to_string(server.port()) + " (Ctrl+C stops it)");
  log::info("the game's saves are kept in " + (settings.data_dir / kSaveFileName).string());
  serve_until(server, [&signals] { return signals.asked(); });
  server.stop();
  log::info("game server stopped");
  return kExitSuccess;
}

}  // namespace

void serve_until(GameServer& server, const std::function<bool()>& stop) {
  TickPacer pacer(config::kFixedDeltaSeconds, seconds_now());
  Census census;
  while (!stop()) {
    std::this_thread::sleep_for(std::chrono::duration<double>(pacer.until_next(seconds_now())));
    server.tick(pacer.take_turn(seconds_now()));
    census.report(server);
  }
}

int run_server_program(const std::vector<std::string>& args) {
  const ServerCommand command = parse_server_command(args);
  if (!command.ok) {
    std::cerr << command.error << "\n\n" << server_usage();
    return kExitUsage;
  }
  if (command.help) {
    std::cout << server_usage();
    return kExitSuccess;
  }
  return run_server(command.settings);
}

}  // namespace redgem::server
