#pragma once

// A character's own animation between machines (NetworkSync): see animation_sync_bindings.cpp.

#include <mruby.h>

#include <optional>

#include "net/pose_baselines.hpp"

namespace redgem {

// What the own animation of `object` (a GameObject) does, as its pose carries it to the others;
// nothing for nil, or when a clip's name is longer than a pose carries.
std::optional<net::Animated> animated_of(mrb_state* mrb, mrb_value object);
// GameObject#__animation_sync.
void install_animation_sync_bindings(mrb_state* mrb);

}  // namespace redgem
