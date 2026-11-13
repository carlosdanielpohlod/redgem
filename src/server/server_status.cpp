#include "server/server_status.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace redgem::server {

namespace {

// The status's names (docs/dedicated-server.md shows them).
constexpr const char* kEngineVersion = "engine_version";
constexpr const char* kUptime = "uptime_seconds";
constexpr const char* kRooms = "rooms";
constexpr const char* kOpen = "open";
constexpr const char* kLimit = "limit";
constexpr const char* kList = "list";
constexpr const char* kPlayers = "players";
constexpr const char* kTotal = "total";
constexpr const char* kLimitPerRoom = "limit_per_room";
constexpr const char* kFrameMs = "frame_ms";
constexpr const char* kAverage = "average";
constexpr const char* kWorst = "worst";
constexpr const char* kBudget = "budget";
constexpr const char* kBytesPerSecond = "bytes_per_second";
constexpr const char* kIn = "in";
constexpr const char* kOut = "out";
constexpr const char* kThreads = "threads";
constexpr const char* kRunning = "running";

constexpr double kMillisecondsPerSecond = 1000.0;
// Numbers are given to a thousandth (of a millisecond, a second, a byte a second): finer says nothing.
constexpr double kThousandths = 1000.0;
constexpr int kLineDecimals = 1;

double rounded(double value) { return std::round(value * kThousandths) / kThousandths; }
double milliseconds(double seconds) { return rounded(seconds * kMillisecondsPerSecond); }

nlohmann::json frame_json(const FrameTimes& frames) {
  return {{kAverage, milliseconds(frames.average_seconds())}, {kWorst, milliseconds(frames.worst_seconds)}};
}

nlohmann::json rooms_json(const ServerStatus& status) {
  nlohmann::json list = nlohmann::json::array();
  for (const RoomStatus& room : status.rooms)
    list.push_back({{kPlayers, room.players}, {kFrameMs, frame_json(room.frames)}});
  return {{kOpen, status.rooms.size()}, {kLimit, status.max_rooms}, {kList, std::move(list)}};
}

nlohmann::json all_frames_json(const ServerStatus& status) {
  nlohmann::json frames = frame_json(all_frames(status));
  frames[kBudget] = milliseconds(status.tick_seconds);
  return frames;
}

std::string fixed(double value) {
  std::ostringstream text;
  text << std::fixed << std::setprecision(kLineDecimals) << value;
  return text.str();
}

}  // namespace

std::size_t total_players(const ServerStatus& status) {
  std::size_t players = 0;
  for (const RoomStatus& room : status.rooms) players += room.players;
  return players;
}

FrameTimes all_frames(const ServerStatus& status) {
  FrameTimes frames;
  for (const RoomStatus& room : status.rooms) frames.add(room.frames);
  return frames;
}

nlohmann::json status_json(const ServerStatus& status) {
  return {{kEngineVersion, status.engine_version},
          {kUptime, rounded(status.uptime_seconds)},
          {kRooms, rooms_json(status)},
          {kPlayers, {{kTotal, total_players(status)}, {kLimitPerRoom, status.max_players}}},
          {kFrameMs, all_frames_json(status)},
          {kBytesPerSecond, {{kIn, rounded(status.traffic.received)}, {kOut, rounded(status.traffic.sent)}}},
          {kThreads, {{kRunning, status.threads}, {kLimit, status.max_threads}}}};
}

std::string status_line(const ServerStatus& status) {
  const FrameTimes frames = all_frames(status);
  return "status: " + std::to_string(status.rooms.size()) + "/" + std::to_string(status.max_rooms) + " rooms, " +
         std::to_string(total_players(status)) + " players, frame " +
         fixed(frames.average_seconds() * kMillisecondsPerSecond) + " ms average, " +
         fixed(frames.worst_seconds * kMillisecondsPerSecond) + " ms worst (of " +
         fixed(status.tick_seconds * kMillisecondsPerSecond) + "), " + fixed(status.traffic.received) + " B/s in, " +
         fixed(status.traffic.sent) + " B/s out, up " + fixed(status.uptime_seconds) + " s";
}

}  // namespace redgem::server
