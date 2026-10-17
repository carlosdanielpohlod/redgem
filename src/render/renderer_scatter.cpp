// Copies scattered in great numbers (Scatter, scene/scatter.hpp): their records, written once
// whenever the scene's sets change, cell by cell, and the cells each pass sees.

#include "render/renderer_impl.hpp"

#include "render/instance_records.hpp"

namespace redgem {

void Renderer::Impl::lay_out_scattered(const Scattered& scattered) {
  if (scattered.version() == scatter_version) return;
  scatter_version = scattered.version();
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
  const render_math::InstanceLook look{set.tint, render_math::kNoBones,
                                       set.sways ? render_math::kSways : render_math::kStill};
  for (std::size_t at = 0; at < order.size(); ++at) {
    render_math::write_instance(texels, first + at, render_math::model_matrix(set.places[order[at]]), look);
  }
  return first + order.size();
}

void Renderer::Impl::gather_scattered(const render_math::View& view) {
  for (const render_math::ScatterLayout& layout : scatter_layouts) render_math::gather_set(batches, view, layout);
}

}  // namespace redgem
