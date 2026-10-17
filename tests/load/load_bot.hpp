#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "net/connection.hpp"
#include "net/pose_baselines.hpp"

namespace redgem::load {

// What one bot said and heard while it was being measured.
struct BotCounts {
  std::size_t bytes_sent = 0;
  std::size_t bytes_received = 0;
  std::size_t poses_sent = 0;
  std::size_t pose_bytes = 0;  // of the poses sent, framing included
  std::size_t messages_sent = 0;
  std::size_t message_bytes = 0;  // of the game messages sent, framing included
  std::size_t poses_heard = 0;
  std::vector<double> latencies_ms;  // of the game messages heard, from when their sender sent them
};

// A player that joins a dedicated server's room by its address, as a game does, and plays like
// one: it runs in circles, its NetworkSync sending where it is SENDS_PER_SECOND times a second (in
// poses that say what changed, see net/pose_codec.hpp), and fires a shot (a game message) a few
// times a second. Nothing here waits: tick() it often.
class LoadBot {
 public:
  explicit LoadBot(int number) : number_(number) {}

  bool connect(std::uint16_t port, std::string& error);
  // Reads what arrived and says what is due at `now` (seconds on the steady clock).
  void tick(double now);
  bool welcomed() const { return id_ != 0; }
  bool gone() const { return connection_.closed(); }
  const BotCounts& counts() const { return counts_; }
  void reset_counts() { counts_ = BotCounts(); }

 private:
  void hear(double now);
  void hear_message(const nlohmann::json& message, double now);
  void move(double now);
  void shoot(double now);
  std::size_t send(const nlohmann::json& message);

  int number_;
  int id_ = 0;
  net::Connection connection_;
  net::PoseSender poses_;
  double next_pose_ = 0.0;
  double next_shot_ = 0.0;
  BotCounts counts_;
};

}  // namespace redgem::load
