// GameServer: how busy it is, on its status port and in its log.

#include "app/crash_report.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "net/traffic.hpp"
#include "server/game_server.hpp"
#include "server/tick_pacer.hpp"

namespace redgem::server {

bool GameServer::open_status(std::string& error) {
  const StatusSettings& wanted = settings_.status;
  if (!wanted.port) return true;
  if (!status_port_.open(wanted.address, *wanted.port, error)) {
    error = "cannot open the status port: " + error;
    return false;
  }
  log::info("status on " + wanted.address + ":" + std::to_string(status_port_.port()));
  return true;
}

ServerStatus GameServer::status() const {
  ServerStatus status;
  status.engine_version = crash::engine_version();
  status.uptime_seconds = steady_seconds() - started_at_;
  status.max_rooms = settings_.limits.max_rooms;
  status.max_players = settings_.limits.max_players;
  status.tick_seconds = config::kFixedDeltaSeconds;
  status.threads = threads_.size();
  status.max_threads = settings_.threads;
  status.traffic = traffic_.per_second();
  for (const Room& room : rooms_) status.rooms.push_back({room.slot->players(), room.slot->frame_times()});
  return status;
}

void GameServer::log_status() const { log::info(status_line(status())); }

void GameServer::report(float seconds) {
  traffic_.note(steady_seconds(), net::traffic_totals());
  status_port_.serve(seconds, [this] { return status_json(status()).dump(); });
  log_when_due(seconds);
}

void GameServer::log_when_due(float seconds) {
  if (settings_.status.log_every_seconds == 0) return;
  since_status_logged_ += seconds;
  if (since_status_logged_ < static_cast<float>(settings_.status.log_every_seconds)) return;
  since_status_logged_ = 0.0f;
  log_status();
}

}  // namespace redgem::server
