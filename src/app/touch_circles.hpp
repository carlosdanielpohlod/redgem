#pragma once

#include <vector>

#include "input/touch_controls.hpp"
#include "render/hud.hpp"

namespace redgem {

// A touch screen's controls as they are drawn: the stick's ring first, then what lies over it
// (the knob, the buttons), each brighter while a finger is on it.
std::vector<ControlCircle> control_circles(const TouchLayout& layout);

}  // namespace redgem
