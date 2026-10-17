// Copies scattered in great numbers (Scatter, scene/scatter.hpp): their records, written once
// whenever the scene's sets change, cell by cell, and the cells each pass sees.

#include "render/renderer_impl.hpp"

#include <utility>

#include "render/instance_records.hpp"

namespace redgem {

using render_detail::ScatterDraw;

void Renderer::Impl::lay_out_scattered(const Scattered& scattered) {
  if (scattered.version() == scatter_version) return;
  scatter_version = scattered.version();
  scatter_draws.clear();
  std::size_t next = 0;
  for (const ScatterSet& set : scattered.sets()) next = lay_out_set(set, next);
  scattered_records = next;
}

std::size_t Renderer::Impl::lay_out_set(const ScatterSet& set, std::size_t first) {
  if (!is_index_of(set.mesh, meshes.size())) return first;
  std::vector<std::size_t> order;
  const Bounds& bounds = mesh_bounds[static_cast<std::size_t>(set.mesh)];
  ScatterDraw draw{set.mesh, set.visible_from, set.visible_to, render_math::chunk_copies(set.places, bounds, order)};
  for (render_math::ScatterChunk& chunk : draw.chunks) chunk.first += first;
  const render_math::InstanceLook look{set.tint, render_math::kNoBones};
  for (std::size_t at = 0; at < order.size(); ++at) {
    render_math::write_instance(texels, first + at, render_math::model_matrix(set.places[order[at]]), look);
  }
  scatter_draws.push_back(std::move(draw));
  return first + order.size();
}

void Renderer::Impl::gather_scattered(const render_math::View& view) {
  for (const ScatterDraw& draw : scatter_draws) {
    for (const render_math::ScatterChunk& chunk : draw.chunks) {
      if (render_math::chunk_worth_drawing(view, chunk, draw.visible_from, draw.visible_to)) {
        batches.add_run(draw.mesh, chunk.first, chunk.count);
      }
    }
  }
}

}  // namespace redgem
