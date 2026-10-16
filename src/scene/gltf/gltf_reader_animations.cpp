// Animation clips of a glTF file.

#include "core/safe_path.hpp"
#include "scene/gltf/gltf_accessors.hpp"
#include "scene/gltf/gltf_reader.hpp"

namespace redgem {
namespace {

// What a channel animates; nullopt for morph target weights, which are not supported.
std::optional<AnimationPath> path_of(cgltf_animation_path_type type) {
  switch (type) {
    case cgltf_animation_path_type_translation:
      return AnimationPath::Translation;
    case cgltf_animation_path_type_rotation:
      return AnimationPath::Rotation;
    case cgltf_animation_path_type_scale:
      return AnimationPath::Scale;
    default:
      return std::nullopt;
  }
}

// A channel that says both what it moves and with which keys.
bool is_whole(const cgltf_animation_channel& channel) {
  return channel.target_node != nullptr && channel.sampler != nullptr;
}

bool has_cubic_spline_keys(const cgltf_animation_channel& channel) {
  return channel.sampler->interpolation == cgltf_interpolation_type_cubic_spline;
}

}  // namespace

void GltfReader::load_animations() {
  const std::vector<int> nodes = node_indices();
  for (cgltf_size i = 0; i < data_.animations_count; ++i) {
    AnimationDesc clip = load_animation(data_.animations[i], i, nodes);
    if (!clip.channels.empty()) result_.scene.animations.push_back(std::move(clip));
  }
}

AnimationDesc GltfReader::load_animation(const cgltf_animation& animation, cgltf_size index,
                                         const std::vector<int>& nodes) {
  AnimationDesc out;
  out.name = printable(text_or(animation.name, "animation_" + std::to_string(index)));
  for (cgltf_size c = 0; c < animation.channels_count; ++c) {
    std::optional<AnimationChannel> track = load_channel(animation.channels[c], out.name, nodes);
    if (!track) continue;
    out.duration = std::max(out.duration, track->times.back());
    out.channels.push_back(std::move(*track));
  }
  return out;
}

std::optional<AnimationChannel> GltfReader::load_channel(const cgltf_animation_channel& channel,
                                                         const std::string& clip_name, const std::vector<int>& nodes) {
  const std::optional<AnimationPath> path = playable_path(channel, clip_name);
  if (!path) return std::nullopt;
  AnimationChannel track;
  track.path = *path;
  track.node = nodes[static_cast<std::size_t>(index_of(channel.target_node, data_.nodes))];
  if (track.node == kNoIndex) return std::nullopt;
  track.times = gltf_accessors::unpack_floats(channel.sampler->input);
  track.values = gltf_accessors::unpack_floats(channel.sampler->output);
  if (track.times.empty()) return std::nullopt;
  return track;
}

std::optional<AnimationPath> GltfReader::playable_path(const cgltf_animation_channel& channel,
                                                       const std::string& clip_name) {
  if (!is_whole(channel)) return std::nullopt;
  const std::optional<AnimationPath> path = path_of(channel.target_path);
  if (!path) return std::nullopt;
  if (!has_cubic_spline_keys(channel)) return path;
  warn("animation '" + clip_name + "': cubic spline keys are not supported, skipped");
  return std::nullopt;
}

}  // namespace redgem
