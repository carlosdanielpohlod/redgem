#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "core/math.hpp"
#include "net/pose_codec.hpp"

// What each side remembers of the poses sent (pose_codec.hpp), so that one says only what changed
// and the other makes whole poses of it again. Messages arrive whole and in order, so what the
// others know of an object is exactly what was sent of it: a move counted from the last pose sent
// adds up to the same place on every machine that heard the whole pose before it. A machine that
// did not (it just arrived) waits for the next whole pose, which comes at once (the sender starts
// over whenever someone joins) and every kSendsPerWholePose anyway.
namespace redgem::net {

// How often an object's whole pose is sent again, at the least: in sends of it (NetworkSync sends
// twenty a second).
inline constexpr int kSendsPerWholePose = 40;
// The objects each side keeps poses of. Past that it forgets them all and starts over from whole
// poses (a game that spawns and destroys many objects would otherwise fill it up).
inline constexpr std::size_t kMaxRememberedPoses = 4096;

// The sender's side: what the others were last told of each of this machine's objects.
class PoseSender {
 public:
  // What takes the others from what they were last told of `key` to this pose.
  PoseUpdate update(const std::string& key, Vec3 position, Quat rotation);
  // The next pose of every object is whole (someone arrived who knows none of them).
  void start_over() { sent_.clear(); }

 private:
  struct Sent {
    Vec3 position;
    std::uint32_t rotation = 0;
    int sends = 1;  // since the last whole pose, that one included
  };

  PoseUpdate whole(const std::string& key, Vec3 position, std::uint32_t rotation);
  static PoseUpdate change(const std::string& key, Sent& sent, const PoseSteps& steps, std::uint32_t rotation);
  // Whether the next pose of an object is to be whole: the others know nothing of it yet, or not lately.
  bool due_whole(std::unordered_map<std::string, Sent>::const_iterator sent) const;

  std::unordered_map<std::string, Sent> sent_;
};

// A whole pose, as an object is shown.
struct Pose {
  Vec3 position;
  Quat rotation;
};

// The receiver's side: what each machine last said of each of its objects.
class PoseReceiver {
 public:
  // The pose machine `from` says one of its objects is at now: what it said before, with what
  // changed; nothing while no whole pose of that object came from it yet.
  std::optional<Pose> take(int from, const PoseUpdate& update);
  // Forgets the objects of a machine that left.
  void forget(int peer);

 private:
  struct Known {
    Vec3 position;
    std::uint32_t rotation = 0;
  };
  using Source = std::pair<int, std::string>;  // a machine's id, and its object's key

  // Starts over what is known of an object, from the whole pose that came.
  void start(const Source& source);
  static void apply(const PoseUpdate& update, Known& known);

  std::map<Source, Known> known_;
};

// Both sides of what one machine remembers of poses.
struct PoseBaselines {
  PoseSender sent;
  PoseReceiver heard;
};

}  // namespace redgem::net
