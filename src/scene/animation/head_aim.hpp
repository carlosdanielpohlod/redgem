#pragma once

// Turning a bone (a character's head) toward a point, on top of the pose its clips gave it: the
// look-at of NPCs. Each frame, after the clips and the scripts, every aimed bone turns toward its
// point, no further than its largest angle, and eases in as it starts and out as it stops.

#include <map>

#include "core/math.hpp"
#include "scene/scene.hpp"

namespace redgem {

// The rotation (in the world) of a bone whose world rotation is `world`, turned so that its
// forward (`axis`, in its own space) points toward `toward` (a direction in the world): by
// `weight` (0 to 1) of the way, and never more than `most_radians` off where it pointed.
Quat aimed_rotation(Quat world, Vec3 axis, Vec3 toward, float most_radians, float weight);

class HeadAims {
 public:
  static constexpr float kEaseSeconds = 0.25f;  // to turn all the way toward a point, or back

  // Turns `bone` (part of `character`) toward `target` (a point in the world), at most
  // `most_degrees` off its pose. Its forward is the way the character faces, as it is now.
  void aim(const Scene& scene, ObjectId character, ObjectId bone, Vec3 target, float most_degrees);
  // Lets the bone go back to its pose, easing out.
  void release(const Scene& scene, ObjectId bone);
  // Whether the bone is turned, or on its way back.
  bool aiming(const Scene& scene, ObjectId bone) const;
  // Turns every aimed bone, `dt` seconds on (0 while paused: nothing eases).
  void apply(Scene& scene, float dt);
  // A new scene: no bone is aimed.
  void clear() { aims_.clear(); }

 private:
  struct Aim {
    Vec3 target;
    float most_radians = 0.0f;
    Vec3 axis = Vec3::forward();  // the bone's forward, in its own space
    float weight = 0.0f;          // how far turned toward the point: eases toward 1, or 0 once released
    bool held = true;
    bool turned = false;  // whether `written` is what this left on the bone last frame
    Quat written;         // the bone's local rotation as this left it
    Quat pose;            // the bone's local rotation before it was turned
  };

  // Turns one bone; false once it is eased all the way out.
  static bool turn(Scene& scene, ObjectId bone, Aim& aim, float dt);
  // The bone's pose this frame: what the clips or a script set, or what it had before this turned it.
  static Quat pose_of(const GameObject& bone, const Aim& aim);
  static float eased(const Aim& aim, float dt);

  std::map<ObjectHandle, Aim> aims_;  // by bone
};

}  // namespace redgem
