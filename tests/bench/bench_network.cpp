// A network frame: what a full room's machines say in one send of NetworkSync, every player's
// pose and a few game messages, encoded as they travel (poses, MessagePack, framing) and decoded
// back from the bytes as they arrive.

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "bench/bench_case.hpp"
#include "core/math.hpp"
#include "net/frame_buffer.hpp"
#include "net/pose_baselines.hpp"
#include "net/pose_codec.hpp"
#include "net/wire.hpp"
#include "net/wire_codec.hpp"

namespace redgem::bench {

namespace {

constexpr int kPlayers = 15;  // the most a room holds
constexpr int kShotsPerFrame = 2;
constexpr int kFramesPerSample = 200;
constexpr float kRunRadius = 5.0f;      // meters: each player runs in a circle
constexpr float kTurnPerFrame = 0.05f;  // radians
constexpr float kStandingHeight = 1.0f;
constexpr double kShotPower = 0.75;
constexpr const char* kHeroKey = "hero-";
constexpr const char* kShot = "shot";

using Bytes = std::vector<std::uint8_t>;

// Where player `player` is on frame `frame`.
Vec3 position_at(int player, int frame) {
  const float angle = static_cast<float>(frame) * kTurnPerFrame + static_cast<float>(player);
  return {kRunRadius * std::cos(angle), kStandingHeight, kRunRadius * std::sin(angle)};
}

void append(Bytes& into, const Bytes& more) { into.insert(into.end(), more.begin(), more.end()); }

// Every player's machine, saying where it is and shooting.
struct Room {
  std::vector<net::PoseSender> senders = std::vector<net::PoseSender>(kPlayers);
  int frame = 0;

  Bytes encode_frame() {
    Bytes bytes;
    for (int player = 0; player < kPlayers; ++player) append(bytes, encode_pose(player));
    for (int shot = 0; shot < kShotsPerFrame; ++shot) append(bytes, encode_shot(shot));
    ++frame;
    return bytes;
  }

  Bytes encode_pose(int player) {
    const std::string key = kHeroKey + std::to_string(player);
    const net::PoseUpdate pose = senders[player].update(key, position_at(player, frame), Quat::identity());
    return net::wire::packed(net::wire::pose_message(net::encoded(pose), player + net::kFirstGuestId));
  }

  Bytes encode_shot(int shot) const {
    const nlohmann::json data = {{"power", kShotPower}, {"frame", frame}};
    return net::wire::packed(net::wire::game_message(kShot, data, shot + net::kFirstGuestId, net::kEveryone));
  }
};

// Reads one message of a frame; whether it was what was sent.
bool decode_message(const std::string& frame_bytes) {
  const auto message = net::wire::unpacked(frame_bytes);
  if (!message) return false;
  if ((*message)[net::wire::kKind] != net::wire::kPose) return true;
  return net::decoded((*message)[net::wire::kData].get_binary()).has_value();
}

void decode_frame(const Bytes& bytes) {
  net::FrameBuffer incoming;
  incoming.append(bytes.data(), bytes.size());
  int messages = 0;
  while (const auto next = incoming.next()) {
    require(decode_message(*next), "decoding a message");
    ++messages;
  }
  require(messages == kPlayers + kShotsPerFrame, "decoding a whole frame");
}

Sampler encode_sampler() {
  auto room = std::make_shared<Room>();
  return [room] { return microseconds_each(kFramesPerSample, [&room] { keep(room->encode_frame().size()); }); };
}

Sampler decode_sampler() {
  Room room;
  room.encode_frame();  // the first poses of each player are whole: a frame of moves is what is common
  auto bytes = std::make_shared<Bytes>(room.encode_frame());
  return [bytes] { return microseconds_each(kFramesPerSample, [&bytes] { decode_frame(*bytes); }); };
}

std::string frame_words() {
  return " a frame of " + std::to_string(kPlayers) + " poses and " + std::to_string(kShotsPerFrame) + " game messages";
}

}  // namespace

std::vector<BenchCase> network_cases() {
  return {{"net.frame_encode", "encoding" + frame_words(), encode_sampler},
          {"net.frame_decode", "decoding" + frame_words(), decode_sampler}};
}

}  // namespace redgem::bench
