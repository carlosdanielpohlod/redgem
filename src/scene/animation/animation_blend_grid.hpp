#pragma once

#include "scene/animation/animation_graph_player.hpp"

namespace redgem {

// Sorts and indexes a complete rectangular grid once. One-dimensional states are left alone.
bool compile_blend_grid(CompiledAnimationState& state);
std::vector<AnimationGraphSample> grid_samples(const CompiledAnimationState& state, float first, float second,
                                               double phase);

}  // namespace redgem
