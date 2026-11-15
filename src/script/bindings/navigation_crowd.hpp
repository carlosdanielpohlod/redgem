#pragma once

// What the Navigation natives share with those of crowds (navigation_crowd_bindings.cpp): the
// running scene's navigation, and the bodies that move as obstacles a path goes around.

#include <mruby.h>

#include <cstddef>
#include <vector>

#include "navigation/navigation.hpp"

namespace redgem::binding {

// The most bodies one path goes around: a script that hands more has the rest left out.
inline constexpr std::size_t kMaxAvoidedBodies = 64;

// The running scene's navigation, its grid baked first when it is not yet.
const Navigation& baked_navigation(mrb_state* mrb);

// The bodies of a list of game objects, as obstacles a path goes around (those without a body, or
// destroyed, are left out).
std::vector<NavObstacle> obstacles_arg(mrb_state* mrb, mrb_value objects);

// Navigation.__steer.
void install_navigation_crowd_bindings(mrb_state* mrb, RClass* module);

}  // namespace redgem::binding
