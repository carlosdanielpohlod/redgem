#pragma once

#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

#include "physics/physics_world.hpp"
#include "scene/scene.hpp"

namespace redgem {

// Lag compensation, on the host: where the objects the network keeps in step (NetworkSync) were
// shown a moment ago, with every body that hangs from them (hit boxes, the bones' ones too), so that
// a shot is judged against what its shooter saw (Physics.rewind). Each frame keeps, for each such
// object, the host time it was shown at (Network.time: now for the host's own objects, the moment
// of the poses it shows for the others') and where it and its bodies were. A rewind puts them, in
// the scene and in the physics, where they were when shown at the time asked; a restore puts them
// back.
class LagHistory {
 public:
  // How far back a rewind may go, in seconds (game.json's network.max_rewind_ms).
  void set_longest(double seconds) { longest_ = seconds; }
  double longest() const { return longest_; }
  // Keeps the history of `id` (and what hangs from it) from now on.
  void track(const Scene& scene, ObjectId id);
  // `id` was shown at host time `at` this frame (else it counts as shown now).
  void shown_at(ObjectId id, double at) { shown_[id] = at; }
  // The end of a frame at host time `now`: keeps where every object kept was.
  void record(const Scene& scene, const PhysicsWorld& physics, double now);
  // Forgets everything (a new scene, or the game is over).
  void clear();

  // The time a rewind asked at `asked` goes to: never after `now`, nor longer ago than longest().
  double clamped(double asked, double now) const;
  // Puts every object kept where it was when shown at `time` (see clamped). False while rewound.
  bool rewind(Scene& scene, PhysicsWorld& physics, double time);
  // Puts them back where they are.
  void restore(Scene& scene, PhysicsWorld& physics);
  bool rewound() const { return rewound_; }

 private:
  // Where one object kept was at a frame's end, and the host time it was shown at.
  struct Sample {
    ObjectId root = kNoObject;
    double shown_at = 0.0;
    std::vector<std::pair<ObjectId, Transform>> poses;  // its own first, then each body under it
  };
  struct Moment {
    double now = 0.0;
    std::vector<Sample> samples;
  };
  // Where one object goes in a rewind, and how many it hangs from.
  struct Placement {
    int depth = 0;
    ObjectId id = kNoObject;
    Transform world;
  };

  Sample sample_of(const Scene& scene, const PhysicsWorld& physics, ObjectId root, double now) const;
  void forget_older_than(double now);
  // Where `root`'s poses were when it was shown at `time`: between the two frames around it.
  std::vector<std::pair<ObjectId, Transform>> poses_at(ObjectId root, double time) const;
  std::vector<Placement> planned(const Scene& scene, double time) const;

  std::vector<ObjectHandle> tracked_;
  std::unordered_map<ObjectId, double> shown_;  // this frame's
  std::deque<Moment> moments_;                  // oldest first
  std::vector<std::pair<ObjectId, Transform>> kept_;  // the locals of what a rewind moved
  std::vector<ObjectId> moved_bodies_;
  double longest_ = 0.0;
  bool rewound_ = false;
};

}  // namespace redgem
