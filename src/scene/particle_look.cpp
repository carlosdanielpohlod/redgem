#include "scene/particle_look.hpp"

#include <cmath>

namespace redgem {

int flipbook_frame(const Flipbook& book, float age, float life) {
  const int frames = book.frames();
  if (book.fps > 0.0f) return static_cast<int>(std::floor(age * book.fps)) % frames;
  const float lived = life > 0.0f ? age / life : 1.0f;
  return std::clamp(static_cast<int>(lived * static_cast<float>(frames)), 0, frames - 1);
}

}  // namespace redgem
