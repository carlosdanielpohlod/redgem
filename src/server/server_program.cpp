#include "server/server_program.hpp"

#include <iostream>

#include "core/config.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "server/server_command.hpp"
#include "server/status_signal.hpp"
#include "server/stop_signals.hpp"
#include "server/tick_pacer.hpp"

namespace redgem::server {

namespace {

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

// Says where a server that started listens, where it keeps its saves and TLS identity, and the
// fingerprint games pin it by.
void say_started(const GameServer& server, const ServerSettings& settings) {
  log::info("game server listening on port " + std::to_string(server.port()) + " (Ctrl+C stops it)");
  log::info("the game's saves are kept in " + (settings.data_dir / kSaveDatabaseName).string());
  log::info("its TLS identity is kept in " + settings.data_dir.string());
  log::info(net::fingerprint_notice(server.fingerprint()));
}

int run_server(const ServerSettings& settings) {
  // (Heard from the start: the system's own handling of SIGUSR1 ends the program.)
  StatusSignal status_signal;
  GameServer server(settings);
  std::string error;
  if (!server.start(error)) {
    log::error("cannot serve " + settings.game_dir.string() + ": " + error);
    return kExitFailure;
  }
  const StopSignals signals;
  say_started(server, settings);
  serve_until(server, [&signals] { return signals.asked(); }, [&status_signal] { return status_signal.taken(); });
  server.stop();
  log::info("game server stopped");
  return kExitSuccess;
}

}  // namespace

void serve_until(GameServer& server, const std::function<bool()>& stop, const std::function<bool()>& status_asked) {
  Clock& clock = server.clock();
  TickPacer pacer(config::kFixedDeltaSeconds, clock.now());
  Census census;
  while (!stop()) {
    clock.sleep(pacer.until_next(clock.now()));
    server.tick(pacer.take_turn(clock.now()));
    census.report(server);
    if (status_asked()) server.log_status();
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
  if (command.show_fingerprint) {
    return net::print_fingerprint(command.settings.data_dir, std::cout) ? kExitSuccess : kExitFailure;
  }
  return run_server(command.settings);
}

}  // namespace redgem::server
