#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "server/frame_window.hpp"
#include "server/traffic_rate.hpp"

// What a dedicated server says about how busy it is, for whoever runs it to know when to give it
// more rooms or a bigger machine: on its status port (see status_port.hpp) and in its log. Counts
// and times only: no room code, no address, nothing of a game or of who plays it.
namespace redgem::server {

// One open room: how many play in it, and how long its recent frames took.
struct RoomStatus {
  std::size_t players = 0;
  FrameTimes frames;
};

struct ServerStatus {
  std::string engine_version;
  double uptime_seconds = 0.0;
  int max_rooms = 0;
  int max_players = 0;        // in one room
  double tick_seconds = 0.0;  // the time a frame has: one taking longer makes its room late
  std::size_t threads = 0;    // the threads the rooms run on now
  int max_threads = 0;
  std::vector<RoomStatus> rooms;  // the open ones, in the order they opened
  BytesPerSecond traffic;
};

std::size_t total_players(const ServerStatus& status);
// Every room's recent frames together.
FrameTimes all_frames(const ServerStatus& status);

// The status as one JSON document (docs/dedicated-server.md shows it).
nlohmann::json status_json(const ServerStatus& status);
// The same, shorter, on one line of the log.
std::string status_line(const ServerStatus& status);

}  // namespace redgem::server
