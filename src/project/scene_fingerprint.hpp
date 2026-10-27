#pragma once

#include <string>

#include "scene/scene_desc.hpp"

namespace redgem::tools::snapshot {

// Content keys ignore glTF array positions and storage widths. Float bits use a fixed byte order.
std::string mesh_key(const MeshDesc& mesh);
std::string image_key(const ImageData& image);
std::string bind_key(const SkinDesc& skin);
std::string channel_key(const AnimationChannel& channel);

}  // namespace redgem::tools::snapshot
