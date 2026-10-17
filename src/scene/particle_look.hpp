#pragma once

// What a particle looks like over its life: how big, in what color and how see-through it is from
// its birth to its end, which picture of a sprite sheet it shows, and how it meets the light and
// what is behind it. No window and no graphics card needed, so it can be tested.

#include <algorithm>
#include <array>

#include "core/math.hpp"

namespace redgem {

inline constexpr int kNoParticleImage = -1;
inline constexpr int kMaxLifeKeys = 4;  // the most values a particle goes through over its life
// What one value becomes (see held_then): as particles always did, it holds for the first half of
// the life, then a size shrinks to a share of itself and an alpha fades to nothing.
inline constexpr float kEndSizeShare = 0.3f;

// A value over a particle's life: `count` keys spread evenly from its birth (the first) to its end
// (the last), going in a straight line from one to the next. One key holds all life long.
template <typename Value>
struct OverLife {
  std::array<Value, kMaxLifeKeys> keys{};
  int count = 1;

  // The value once `lived` (0 at birth, 1 at the end) of the life is over.
  Value at(float lived) const {
    const float along = std::clamp(lived, 0.0f, 1.0f) * static_cast<float>(count - 1);
    const int before = std::min(static_cast<int>(along), count - 1);
    const int after = std::min(before + 1, count - 1);
    const float between = along - static_cast<float>(before);
    const Value& from = keys[static_cast<std::size_t>(before)];
    return from + (keys[static_cast<std::size_t>(after)] - from) * between;
  }
};

// A value that holds for the first half of the life and then goes to `end`.
inline constexpr int kHeldThenKeys = 3;  // the start, the start again half way, the end
template <typename Value>
OverLife<Value> held_then(Value start, Value end) {
  return OverLife<Value>{{start, start, end}, kHeldThenKeys};
}
template <typename Value>
OverLife<Value> steady(Value value) {
  return OverLife<Value>{{value}, 1};
}

// A picture made of several, in a grid of `columns` x `rows` cells read like a page (left to
// right, top to bottom): a flame that licks, smoke that rolls. `fps` cells a second, round and
// round; 0 shows them all once over the particle's life.
struct Flipbook {
  int columns = 1;
  int rows = 1;
  float fps = 0.0f;

  int frames() const { return columns * rows; }
};

// The cell a particle of `age` seconds, out of a life of `life`, shows.
int flipbook_frame(const Flipbook& book, float age, float life);

struct ParticleLook {
  OverLife<float> size = steady(0.0f);         // meters across
  OverLife<Vec3> color = steady(Vec3::one());  // red, green, blue from 0 to 1, as color pickers show them
  OverLife<float> alpha = steady(1.0f);
  bool glow = false;             // lit from within: adds light instead of covering what is behind
  bool lit = false;              // takes the light of the sun, the sky and the lights near it (smoke, dust)
  float soft = 0.0f;             // meters over which it fades where it meets what is behind it (0: a hard edge)
  int image = kNoParticleImage;  // index into ParticleSystem::images()
  Flipbook flipbook;

  // Glowing particles shine with their own light, lit or not.
  bool takes_light() const { return lit && !glow; }
};

// White, `size` meters across, holding and then shrinking and fading out (see held_then).
inline ParticleLook fading_look(float size) {
  ParticleLook look;
  look.size = held_then(size, size * kEndSizeShare);
  look.alpha = held_then(1.0f, 0.0f);
  return look;
}

}  // namespace redgem
