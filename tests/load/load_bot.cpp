#include "load/load_bot.hpp"

#include <cmath>
#include <cstdint>
#include <utility>

#include "core/math.hpp"
#include "net/wire.hpp"

namespace redgem::load {

namespace {

constexpr const char* kLocalHost = "127.0.0.1";
constexpr double kPosesPerSecond = 20.0;  // NetworkSync::SENDS_PER_SECOND
constexpr double kShotsPerSecond = 2.0;
constexpr double kRunRadius = 5.0;       // meters: a circle run at a jog
constexpr double kTurnsPerSecond = 1.0;  // radians a second around it (five meters a second)
constexpr double kBotSpacing = 12.0;     // meters between the middles of two bots' circles
constexpr double kBobHeight = 0.25;      // meters up and down as it runs
constexpr double kStandingHeight = 1.0;
constexpr double kShotPower = 0.75;
constexpr double kMicroseconds = 1e6;
constexpr double kMilliseconds = 1e3;
constexpr const char* kShot = "shot";
constexpr const char* kSentKey = "sent";
// What a player's hero goes by on the network in the platformer (CoOp::HERO_KEY_PREFIX and the
// player's id): the server's game moves its copy of the hero as the bot says.
constexpr const char* kHeroKey = "hero-";

// Where bot `number` is at `now`, and which way it faces (degrees about Y, as Ruby reads them).
Vec3 position_at(int number, double now) {
  const double angle = now * kTurnsPerSecond;
  return {static_cast<float>(number * kBotSpacing + kRunRadius * std::cos(angle)),
          static_cast<float>(kStandingHeight + kBobHeight * std::sin(angle)),
          static_cast<float>(kRunRadius * std::sin(angle))};
}

Quat rotation_at(double now) {
  return quat_from_euler_degrees({0.0f, radians_to_degrees(static_cast<float>(-now * kTurnsPerSecond)), 0.0f});
}

// A vector as Ruby's to_a gives it, numbers of 64 bits.
nlohmann::json numbers(Vec3 vector) {
  return nlohmann::json::array(
      {static_cast<double>(vector.x), static_cast<double>(vector.y), static_cast<double>(vector.z)});
}

}  // namespace

bool LoadBot::connect(std::uint16_t port, std::string& error) {
  net::Socket socket = net::Socket::connect_to(kLocalHost, port, error);
  if (!socket.valid()) return false;
  connection_ = net::Connection::calling(std::move(socket));
  send(net::wire::hello());
  return true;
}

void LoadBot::tick(double now) {
  counts_.bytes_received += connection_.pump();
  hear(now);
  if (!welcomed()) return;
  if (now >= next_pose_) move(now);
  if (now >= next_shot_) shoot(now);
}

void LoadBot::hear(double now) {
  while (const auto message = connection_.next_message()) hear_message(*message, now);
}

void LoadBot::hear_message(const nlohmann::json& message, double now) {
  const std::string kind = message[net::wire::kKind];
  if (kind == net::wire::kWelcome) id_ = message.value(net::wire::kTo, 0);
  if (kind == net::wire::kArrived) poses_.start_over();  // the newcomer knows none of this bot's poses
  if (kind == net::wire::kPose) ++counts_.poses_heard;
  if (message.value(net::wire::kName, std::string()) != kShot) return;
  const double sent = message[net::wire::kData].value(kSentKey, 0.0) / kMicroseconds;
  counts_.latencies_ms.push_back((now - sent) * kMilliseconds);
}

void LoadBot::move(double now) {
  next_pose_ = now + 1.0 / kPosesPerSecond;
  const net::PoseUpdate pose =
      poses_.update(kHeroKey + std::to_string(id_), position_at(number_, now), rotation_at(now));
  counts_.pose_bytes += send(net::wire::pose_message(net::encoded(pose), id_));
  ++counts_.poses_sent;
}

void LoadBot::shoot(double now) {
  next_shot_ = now + 1.0 / kShotsPerSecond;
  const nlohmann::json shot = {{"at", {{"$v", numbers(position_at(number_, now))}}},
                               {"power", kShotPower},
                               {kSentKey, static_cast<std::int64_t>(now * kMicroseconds)}};
  counts_.message_bytes += send(net::wire::game_message(kShot, shot, id_, net::kEveryone));
  ++counts_.messages_sent;
}

std::size_t LoadBot::send(const nlohmann::json& message) {
  const std::size_t bytes = net::wire::packed(message).size();
  connection_.send(message);
  counts_.bytes_sent += bytes;
  return bytes;
}

}  // namespace redgem::load
