#include "scene/animation/animation_blend_grid.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

constexpr std::size_t kGridCorners = 4;        // maximum samples in one bilinear cell
constexpr std::size_t kMinGridAxisPoints = 2;  // a grid must vary along both axes

bool before(const AnimationGraphPoint& a, const AnimationGraphPoint& b) {
  if (a.second_threshold != b.second_threshold) return a.second_threshold < b.second_threshold;
  return a.threshold < b.threshold;
}

void unique_axis(std::vector<float>& axis) {
  std::sort(axis.begin(), axis.end());
  axis.erase(std::unique(axis.begin(), axis.end()), axis.end());
}

bool finite_coordinates(const AnimationGraphPoint& point) {
  return std::isfinite(point.threshold) && std::isfinite(point.second_threshold);
}

bool collect_axes(CompiledAnimationState& state) {
  state.first_axis.clear();
  state.second_axis.clear();
  for (const auto& point : state.points) {
    if (!finite_coordinates(point)) return false;
    state.first_axis.push_back(point.threshold);
    state.second_axis.push_back(point.second_threshold);
  }
  unique_axis(state.first_axis);
  unique_axis(state.second_axis);
  return true;
}

bool rectangular(const CompiledAnimationState& state) {
  const std::size_t width = state.first_axis.size();
  const std::size_t height = state.second_axis.size();
  if (width < kMinGridAxisPoints || height < kMinGridAxisPoints) return false;
  // Division avoids overflowing a product when validating an untrusted grid.
  return state.points.size() / width == height && state.points.size() % width == 0;
}

bool ordered_coordinates(const CompiledAnimationState& state) {
  const std::size_t width = state.first_axis.size();
  for (std::size_t i = 0; i < state.points.size(); ++i) {
    if (state.points[i].threshold != state.first_axis[i % width]) return false;
    if (state.points[i].second_threshold != state.second_axis[i / width]) return false;
  }
  return true;
}

struct AxisMix {
  std::size_t lower = 0;
  std::size_t upper = 0;
  double weight = 0;
};

AxisMix along(const std::vector<float>& axis, float value) {
  if (value <= axis.front()) return {0, 0, 0};
  if (value >= axis.back()) return {axis.size() - 1, axis.size() - 1, 0};
  const auto next = std::upper_bound(axis.begin(), axis.end(), value);
  const auto upper = static_cast<std::size_t>(next - axis.begin());
  const double span = static_cast<double>(*next) - *(next - 1);
  return {upper - 1, upper, (static_cast<double>(value) - *(next - 1)) / span};
}

void add_sample(std::vector<AnimationGraphSample>& result, const CompiledAnimationState& state, std::size_t index,
                double weight, double phase) {
  const float kept = static_cast<float>(weight);
  if (kept == 0) return;
  const auto& point = state.points[index];
  result.push_back({point.clip, static_cast<float>(phase * point.duration), kept, state.loop});
}

}  // namespace

bool compile_blend_grid(CompiledAnimationState& state) {
  if (state.second_parameter.empty()) return true;
  if (!collect_axes(state)) return false;
  if (!rectangular(state)) return false;
  std::sort(state.points.begin(), state.points.end(), before);
  return ordered_coordinates(state);
}

std::vector<AnimationGraphSample> grid_samples(const CompiledAnimationState& state, float first, float second,
                                               double phase) {
  const AxisMix x = along(state.first_axis, first);
  const AxisMix y = along(state.second_axis, second);
  const std::size_t width = state.first_axis.size();
  std::vector<AnimationGraphSample> result;
  result.reserve(kGridCorners);
  add_sample(result, state, y.lower * width + x.lower, (1 - x.weight) * (1 - y.weight), phase);
  add_sample(result, state, y.lower * width + x.upper, x.weight * (1 - y.weight), phase);
  add_sample(result, state, y.upper * width + x.lower, (1 - x.weight) * y.weight, phase);
  add_sample(result, state, y.upper * width + x.upper, x.weight * y.weight, phase);
  return result;
}

}  // namespace redgem
