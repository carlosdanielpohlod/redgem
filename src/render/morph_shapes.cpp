#include "render/morph_shapes.hpp"

#include <algorithm>
#include <cmath>

#include "scene/scene_config.hpp"

namespace redgem::render_math {
namespace {

constexpr std::size_t kAxes = config::kFloatsPerPosition;  // floats of a position or a delta
constexpr float kNoDelta = 0.0f;
constexpr float kUnusedFloat = 0.0f;  // the fourth float of a delta's texel
constexpr std::size_t kKeyFloat = 0;  // of a (key, weight) pair
constexpr std::size_t kWeightFloat = 1;
constexpr std::size_t kFloatsPerMorph = kFloatsPerTexel / kMorphsPerTexel;

bool heavier(const ActiveMorph& a, const ActiveMorph& b) { return std::fabs(a.weight) > std::fabs(b.weight); }

// Whether `needed` texels of deltas (some) fit from `first` on within `max_texels`.
bool room_for(std::size_t first, std::size_t needed, std::size_t max_texels) {
  return needed > 0 && first + needed <= max_texels;
}

bool shows(float weight) { return std::fabs(weight) >= kSmallestWeight; }

// Appends the delta of vertex `vertex` in `deltas` (xyz per vertex; none when empty) as one texel.
void append_delta(std::vector<float>& texels, const std::vector<float>& deltas, std::size_t vertex) {
  const std::size_t at = vertex * kAxes;
  for (std::size_t axis = 0; axis < kAxes; ++axis) {
    texels.push_back(deltas.empty() ? kNoDelta : deltas[at + axis]);
  }
  texels.push_back(kUnusedFloat);
}

void append_target(std::vector<float>& texels, const MorphTarget& target, std::size_t vertices) {
  for (std::size_t v = 0; v < vertices; ++v) {
    append_delta(texels, target.positions, v);
    append_delta(texels, target.normals, v);
  }
}

}  // namespace

std::vector<ActiveMorph> active_morphs(const std::vector<float>& weights) {
  std::vector<ActiveMorph> active;
  for (std::size_t key = 0; key < weights.size(); ++key) {
    if (shows(weights[key])) active.push_back({key, weights[key]});
  }
  std::stable_sort(active.begin(), active.end(), heavier);
  if (active.size() > kMaxActiveMorphs) active.resize(kMaxActiveMorphs);
  return active;
}

bool shows_morphs(const std::vector<float>& weights) { return std::any_of(weights.begin(), weights.end(), shows); }

MorphTexels morph_texels(const std::vector<ActiveMorph>& active) {
  MorphTexels texels{};
  const std::size_t count = std::min(active.size(), kMaxActiveMorphs);
  for (std::size_t i = 0; i < count; ++i) {
    texels[i * kFloatsPerMorph + kKeyFloat] = static_cast<float>(active[i].key);
    texels[i * kFloatsPerMorph + kWeightFloat] = active[i].weight;
  }
  return texels;
}

float append_morph_deltas(std::vector<float>& texels, const PrimitiveDesc& part, std::size_t max_texels) {
  const std::size_t vertices = part.positions.size() / kAxes;
  const std::size_t first = texels.size() / kFloatsPerTexel;
  const std::size_t needed = part.targets.size() * vertices * kTexelsPerMorphVertex;
  if (!room_for(first, needed, max_texels)) return kNoMorphs;
  for (const MorphTarget& target : part.targets) append_target(texels, target, vertices);
  return static_cast<float>(first);
}

Vec3 morphed_position(const PrimitiveDesc& part, std::size_t vertex, const std::vector<ActiveMorph>& active) {
  const std::size_t at = vertex * kAxes;
  Vec3 position = vec3_from(&part.positions[at]);
  for (const ActiveMorph& morph : active) {
    if (morph.key < part.targets.size())
      position = position + vec3_from(&part.targets[morph.key].positions[at]) * morph.weight;
  }
  return position;
}

}  // namespace redgem::render_math
