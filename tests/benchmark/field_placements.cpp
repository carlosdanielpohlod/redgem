// Where the benchmark's tufts and trees stand: the same places every run.

#include <cmath>
#include <random>

#include "benchmark/field_scene.hpp"

namespace redgem::benchmark {
namespace {

constexpr float kFullTurn = 360.0f;
constexpr float kGrassScaleLow = 0.7f;
constexpr float kGrassScaleHigh = 1.4f;
constexpr float kTreeScaleLow = 0.8f;
constexpr float kTreeScaleHigh = 1.5f;

struct Dice {
  std::mt19937 engine{kFieldSeed};
  float between(float low, float high) { return std::uniform_real_distribution<float>(low, high)(engine); }
};

}  // namespace

std::vector<Placement> grass_placements() {
  Dice dice;
  std::vector<Placement> placements(kGrassTufts);
  for (Placement& placement : placements) {
    placement.position = {dice.between(-kFieldSize * kHalf, kFieldSize * kHalf), 0.0f,
                          dice.between(-kFieldSize * kHalf, kFieldSize * kHalf)};
    placement.yaw_degrees = dice.between(0.0f, kFullTurn);
    placement.scale = dice.between(kGrassScaleLow, kGrassScaleHigh);
  }
  return placements;
}

std::vector<Placement> tree_placements() {
  Dice dice;
  std::vector<Placement> placements(kTrees);
  for (Placement& placement : placements) {
    const float turn = degrees_to_radians(dice.between(0.0f, kFullTurn));
    const float away = dice.between(kForestInner, kForestOuter);
    placement.position = {std::cos(turn) * away, 0.0f, std::sin(turn) * away};
    placement.yaw_degrees = dice.between(0.0f, kFullTurn);
    placement.scale = dice.between(kTreeScaleLow, kTreeScaleHigh);
  }
  return placements;
}

}  // namespace redgem::benchmark
