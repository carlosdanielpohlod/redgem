// Animation clips of a glTF file.

#include "core/safe_path.hpp"
#include "scene/animation/animation_markers.hpp"
#include "scene/gltf/extras_parser.hpp"
#include "scene/gltf/gltf_accessors.hpp"
#include "scene/gltf/gltf_reader.hpp"

namespace redgem {
namespace {

// What a channel animates; nullopt for what glTF does not name.
std::optional<AnimationPath> path_of(cgltf_animation_path_type type) {
  switch (type) {
    case cgltf_animation_path_type_translation:
      return AnimationPath::Translation;
    case cgltf_animation_path_type_rotation:
      return AnimationPath::Rotation;
    case cgltf_animation_path_type_scale:
      return AnimationPath::Scale;
    case cgltf_animation_path_type_weights:
      return AnimationPath::Weights;
    default:
      return std::nullopt;
  }
}

bool has_parts(const cgltf_node& node) { return node.mesh != nullptr && node.mesh->primitives_count > 0; }

// How many shape keys the mesh of a node has (0 without a mesh).
std::size_t shape_keys_of(const cgltf_node& node) {
  return has_parts(node) ? node.mesh->primitives[0].targets_count : 0;
}

// Whether a weights channel gives one weight for every shape key of its node's mesh at each key.
bool weighs_every_key(const AnimationChannel& track, const cgltf_node& node) {
  const std::size_t keys = shape_keys_of(node);
  return keys > 0 && track.values.size() == track.times.size() * keys;
}

// Whether a channel is whole: weights for every shape key (other paths are checked as they are played).
bool keys_fit(const AnimationChannel& track, const cgltf_node& node) {
  return track.path != AnimationPath::Weights || weighs_every_key(track, node);
}

// A channel that says both what it moves and with which keys.
bool is_whole(const cgltf_animation_channel& channel) {
  return channel.target_node != nullptr && channel.sampler != nullptr;
}

bool has_cubic_spline_keys(const cgltf_animation_channel& channel) {
  return channel.sampler->interpolation == cgltf_interpolation_type_cubic_spline;
}

std::string refused_marker_warning(const std::string& clip_name, const std::string& entry) {
  return "animation '" + clip_name + "': event '" + entry + "' is not name@time with a time from 0 to 1, skipped";
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
  out.markers = load_markers(animation, out.name);
  return out;
}

std::vector<AnimationMarker> GltfReader::load_markers(const cgltf_animation& animation, const std::string& clip_name) {
  if (animation.extras.data == nullptr) return {};
  const ExtrasParseResult extras = parse_extras(animation.extras.data);
  if (!extras.ok) warn("animation '" + clip_name + "': bad extras (" + extras.error + ")");
  const std::optional<std::string> listed = get_string(extras.properties, kAnimationEventsProperty);
  if (!listed) return {};
  ParsedMarkers parsed = parse_animation_markers(*listed);
  for (const std::string& entry : parsed.refused) warn(refused_marker_warning(clip_name, entry));
  return std::move(parsed.markers);
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
  if (keys_fit(track, *channel.target_node)) return track;
  warn("animation '" + clip_name + "': shape key weights that do not fit their mesh, skipped");
  return std::nullopt;
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
