#pragma once

#include <map>
#include <utility>

#include "scene/animation/animation_graph_player.hpp"

namespace redgem {

using AnimationChannelKey = std::pair<int, AnimationPath>;
using AnimationBlendLayout = std::map<AnimationChannelKey, std::vector<const AnimationChannel*>>;

// Clip channels must outlive this index, just as they outlive AnimationPlayer.
AnimationBlendLayout animation_blend_layout(const std::vector<AnimationDesc>& clips, std::size_t nodes);

// Builds a single pose from normalized sample weights. A missing channel contributes its rest
// value, including when no current sample animates a channel that another scene clip contains.
AnimationDesc blended_animation(const std::vector<AnimationDesc>& clips,
                                const std::vector<AnimationGraphSample>& samples, const std::vector<Transform>& rest,
                                const AnimationBlendLayout& layout);

}  // namespace redgem
