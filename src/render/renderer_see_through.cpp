// See-through surfaces (see material_alpha.hpp): after the opaque ones, the parts of blended
// materials that are not nearly opaque, object by object, the farthest first, mixed with what is
// behind them and writing no depth. Scattered copies are drawn first, run by run, unsorted.

#include "render/renderer_impl.hpp"

#include <algorithm>
#include <limits>

namespace redgem {
namespace {

using namespace render_detail;
using render_math::AlphaPass;

constexpr float kFarthest = std::numeric_limits<float>::max();  // scattered copies: behind every object

float distance_squared(Vec3 a, Vec3 b) {
  const Vec3 apart = a - b;
  return dot(apart, apart);
}

}  // namespace

void Renderer::Impl::draw_see_through(std::size_t first, Vec3 viewer) {
  gather_see_through(first, viewer);
  if (see_through.empty()) return;
  render_math::sort_back_to_front(see_through);
  ObjectPass pass{begin_mesh_pass(shader, lit_instance_slot), lit_uniforms.skinned, AlphaPass::SeeThrough, {}};
  rlSetBlendMode(RL_BLEND_ALPHA);
  rlDisableDepthMask();
  for (const render_math::SeeThroughDraw& draw : see_through) draw_see_through_copies(draw, pass);
  rlEnableDepthMask();
  end_mesh_pass();
}

void Renderer::Impl::gather_see_through(std::size_t first, Vec3 viewer) {
  see_through.clear();
  for (const render_math::InstanceBatch& batch : batches.batches()) {
    if (has_see_through(batch.mesh)) add_see_through(batch, first, viewer);
  }
}

bool Renderer::Impl::has_see_through(int mesh) const {
  const std::vector<GpuPrimitive>& parts = meshes[static_cast<std::size_t>(mesh)];
  return std::any_of(parts.begin(), parts.end(), [this](const GpuPrimitive& part) {
    return render_math::drawn_see_through(factors_of(part.material));
  });
}

// The objects of a batch one by one, at the distance of their middle; the scattered copies after
// them (their records come before every object's) in one run.
void Renderer::Impl::add_see_through(const render_math::InstanceBatch& batch, std::size_t first, Vec3 viewer) {
  const std::vector<float>& records = batches.slots();
  std::size_t copy = 0;
  for (; copy < batch.count; ++copy) {
    const auto record = static_cast<std::size_t>(records[batch.first + copy]);
    if (record < scattered_records) break;
    const Vec3 middle = sights[record - scattered_records].sight.sphere.center;
    see_through.push_back({distance_squared(middle, viewer), batch.mesh, first + batch.first + copy, 1});
  }
  if (copy < batch.count)
    see_through.push_back({kFarthest, batch.mesh, first + batch.first + copy, batch.count - copy});
}

void Renderer::Impl::draw_see_through_copies(const render_math::SeeThroughDraw& draw, ObjectPass& pass) {
  const InstanceRun run{slots.id, draw.first, draw.count};
  for (const GpuPrimitive& part : meshes[static_cast<std::size_t>(draw.mesh)]) {
    if (render_math::drawn_see_through(factors_of(part.material))) draw_primitive(part, run, pass);
  }
}

}  // namespace redgem
