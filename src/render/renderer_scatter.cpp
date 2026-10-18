// Copies scattered in great numbers (Scatter, scene/scatter.hpp): their records, written once
// whenever the scene's sets change, cell by cell, and the cells each pass sees.

#include "render/renderer_impl.hpp"

#include "render/instance_records.hpp"

namespace redgem {
namespace {

// A copy's own tint, on top of its set's.
Vec3 copy_tint(const ScatterSet& set, std::size_t copy) {
  return copy < set.tints.size() ? hadamard(set.tint, set.tints[copy]) : set.tint;
}

}  // namespace

void Renderer::Impl::lay_out_scattered(const Scattered& scattered) {
  if (scattered.version() == scatter_version) return;
  scatter_version = scattered.version();
  temporal.history_valid = false;  // every object's record moves: none would find its last frame's
  scatter_layouts.clear();
  std::size_t next = 0;
  for (const ScatterSet& set : scattered.sets()) next = lay_out_set(set, next);
  scattered_records = next;
}

std::size_t Renderer::Impl::lay_out_set(const ScatterSet& set, std::size_t first) {
  if (!is_index_of(set.mesh, meshes.size())) return first;
  std::vector<std::size_t> order;
  const Bounds& bounds = mesh_bounds[static_cast<std::size_t>(set.mesh)];
  scatter_layouts.push_back(render_math::lay_out_copies(set, bounds, first, order));
  render_math::InstanceLook look{set.tint, render_math::kNoBones, set.visible_from, set.visible_to,
                                 set.fade, set.normals_up};
  look.sway = set.sways ? render_math::kSways : render_math::kStill;
  for (std::size_t at = 0; at < order.size(); ++at) {
    look.tint = copy_tint(set, order[at]);
    render_math::write_instance(texels, first + at, render_math::model_matrix(set.places[order[at]]), look);
  }
  return first + order.size();
}

void Renderer::Impl::gather_scattered(const render_math::View& view, bool depth_pass) {
  for (const render_math::ScatterLayout& layout : scatter_layouts) {
    if (render_detail::casts_into(layout.cast_shadow, depth_pass)) render_math::gather_set(batches, view, layout);
  }
}

}  // namespace redgem
