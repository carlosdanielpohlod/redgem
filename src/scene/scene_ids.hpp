#pragma once

#include <map>
#include <string>
#include <vector>

#include "scene/scene_desc.hpp"

namespace redgem {

inline constexpr const char* kSceneIdExtension = "REDGEM_object_id";
inline constexpr const char* kSceneIdMember = "id";

bool valid_scene_id(const std::string& id);
// Legacy scenes get deterministic numeric identities; existing unique identities stay as they are.
std::vector<std::string> assign_scene_ids(SceneDesc& scene);
// A new identity; a document checks it against identities already issued in that document.
std::string make_scene_id();
// Unsaved in-memory descriptions with no identities still have an index-based display fallback.
std::string scene_node_id(const SceneDesc& scene, int index);
// Old identity -> new one, of nodes copied or pasted with new identities.
using NewIdentities = std::map<std::string, std::string>;
// A joint (props::kJointTo) whose other end got a new identity holds the new node: a door copied
// with its frame hangs on the copied frame. Other joints keep their other end.
void follow_new_identities(PropertyMap& props, const NewIdentities& identities);

}  // namespace redgem
