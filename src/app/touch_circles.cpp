#include "app/touch_circles.hpp"

namespace redgem {

std::vector<ControlCircle> control_circles(const TouchLayout& layout) {
  const auto button = [](const TouchCircle& at) {
    return ControlCircle{at.x, at.y, at.radius,
                         at.pressed ? ControlCircle::Look::Pressed : ControlCircle::Look::Resting};
  };
  const TouchCircle& base = layout.stick_base;
  return {{base.x, base.y, base.radius, ControlCircle::Look::Base},
          button(layout.stick_knob),
          button(layout.a),
          button(layout.b),
          button(layout.start)};
}

}  // namespace redgem
