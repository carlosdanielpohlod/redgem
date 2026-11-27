#pragma once

// Pose preview: an object of the edited scene shown as a clip poses it some seconds in, without
// running the game. Only the Scene view's copy of the scene is posed: the document, and so every
// save, keeps the rest pose.

#include <set>
#include <string>
#include <vector>

#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::pose_preview {

// What is posed: an object, a clip of the scene (an index into SceneDesc::animations), and how
// many seconds into it.
struct Pose {
  int object = kNoIndex;
  int clip = kNoIndex;
  float seconds = 0.0f;
};

// The nodes a pose of `object` may move: it and everything under it, and the joints of the
// skeletons its meshes are skinned to (a rig's bones are often beside its mesh, not under it).
std::set<int> posed_nodes(const SceneDesc& desc, int object);
// The clips that move any of those nodes, in the scene's order.
std::vector<int> clips_moving(const SceneDesc& desc, int object);
// A clip's name, or "Clip <n>" for one the file did not name.
std::string clip_name(const SceneDesc& desc, int clip);
// `seconds` within the clip: 0 to its length.
float within_clip(const AnimationDesc& clip, float seconds);
// Whether `pose` can be shown on `desc`: its object and clip are there, and the clip moves it.
bool fits(const SceneDesc& desc, const Pose& pose);
// Poses `scene` (made from `desc`) as `pose` says: only the posed nodes move, and the clip
// holds its last pose past its end.
void show(const SceneDesc& desc, const Pose& pose, Scene& scene);

}  // namespace redgem::pose_preview
