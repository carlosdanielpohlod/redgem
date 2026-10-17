#include "scene/particle_look.hpp"

#include <cmath>

namespace redgem {
namespace {

// How far a particle of `age` seconds is into a flipbook's current turn, from 0 to 1. (Worked out
// on the turn, not on how many cells went by: those can be more than an int holds.)
float share_of_turn(const Flipbook& book, float age) {
  const float turn = static_cast<float>(book.frames()) / book.fps;  // seconds: every cell once
  return std::fmod(age, turn) / turn;
}

float share_of_life(float age, float life) { return life > 0.0f ? age / life : 1.0f; }

}  // namespace

int flipbook_frame(const Flipbook& book, float age, float life) {
  const int frames = book.frames();
  const float share = book.fps > 0.0f ? share_of_turn(book, age) : share_of_life(age, life);
  return std::clamp(static_cast<int>(share * static_cast<float>(frames)), 0, frames - 1);
}

}  // namespace redgem
