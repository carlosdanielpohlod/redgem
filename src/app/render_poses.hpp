#pragma once

#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

#include "physics/physics_world.hpp"
#include "scene/scene.hpp"

namespace redgem {

// Where a frame draws the objects the physics moves on its own (dynamic bodies, characters,
// vehicles): between the poses of their last two steps, as far as the frame is into the next one,
// so that a game drawn more often than it ticks moves smoothly. What they carry (a camera, a gun,
// the wheels) is drawn with them. Scripts never see it: the scene holds the step's pose, but while
// a frame is handed to the presenter.
class RenderPoses {
 public:
  // Before a step's poses are pulled into the scene: where each such object is now (where a
  // script may have put it since the last step).
  void begin_step(const Scene& scene, const PhysicsWorld& physics);
  // After: where each went.
  void end_step(const Scene& scene);
  // Forgets every pose (a new scene).
  void clear();

  // Puts each object still where its last step left it `share` of the way from the pose before
  // to that one (0 to 1; 1 changes nothing), for a frame to be drawn. Until restore().
  void place(Scene& scene, float share);
  // Puts them back where the steps left them.
  void restore(Scene& scene);

  // Where `id` is drawn at that share: on an object placed so (itself, or one it rides on), where
  // that one is drawn carries it; anywhere else, where it is.
  Transform drawn(const Scene& scene, ObjectId id, float share) const;

 private:
  struct Steps {
    ObjectId id = kNoObject;
    Transform before;
    Transform after;
    bool turns = true;  // a character's turning is its scripts', not its body's: only its position
  };

  // Whether the object is still where its step left it: not moved by a script since, not gone.
  bool unmoved(const Scene& scene, const Steps& steps) const;
  // Its pose `share` of the way from before to after, at its scale of now.
  static Transform between(const Steps& steps, const Transform& now, float share);
  // The nearest placed object it is, or rides on; nullptr when none is.
  const Steps* carrier(const Scene& scene, ObjectId id) const;

  std::vector<Steps> steps_;
  std::unordered_map<ObjectId, std::size_t> index_;  // into steps_
  std::vector<std::pair<ObjectId, Transform>> kept_;  // the locals of the objects placed, to put back
};

}  // namespace redgem
