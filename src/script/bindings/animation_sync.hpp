#pragma once

// A character's own animation between machines (NetworkSync): see animation_sync_bindings.cpp.

#include <mruby.h>

#include <optional>
#include <string>

#include "net/pose_baselines.hpp"
#include "scene/animation/character_animation.hpp"

namespace redgem {

// What the own animation of `object` (a GameObject) does, as its pose under `key` carries it to
// the others; nothing for nil, while no script set any, when they heard it already, or when a
// clip's name is not one a pose carries.
std::optional<net::Animated> animated_of(mrb_state* mrb, mrb_value object, const std::string& key);
// GameObject#__animation_sync.
void install_animation_sync_bindings(mrb_state* mrb);
std::optional<ClipState> animation_clip_arg(mrb_state* mrb, mrb_value value);
std::optional<GraphAnimation> animation_graph_arg(mrb_state* mrb, mrb_value value);
net::GraphPart animation_graph_part(const GraphAnimation& graph);

}  // namespace redgem
