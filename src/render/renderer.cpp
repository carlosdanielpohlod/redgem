#include "render/renderer.hpp"

#include <raylib.h>

#include <algorithm>
#include <filesystem>
#include <vector>

#include "render/mesh_pieces.hpp"
#include "render/render_config.hpp"
#include "render/renderer_impl.hpp"

namespace redgem {
namespace {

using render_detail::GpuPrimitive;

constexpr float kNoMotionBlur = 0.0f;
constexpr float kFullMotionBlur = 1.0f;
// raylib's vertex buffer slots for a mesh's positions and normals.
constexpr int kPositionBuffer = 0;
constexpr int kNormalBuffer = 2;

// Same vertices in the same order, only moved: write over the buffers already on the card.
void move_vertices(const Mesh& mesh, const PrimitiveDesc& primitive) {
  if (static_cast<int>(vertex_count_of(primitive)) != mesh.vertexCount) return;
  const int bytes = static_cast<int>(primitive.positions.size() * sizeof(float));
  UpdateMeshBuffer(mesh, kPositionBuffer, primitive.positions.data(), bytes, 0);
  UpdateMeshBuffer(mesh, kNormalBuffer, primitive.normals.data(), bytes, 0);
}

// A minimized window has no size: there is nothing to draw into.
bool has_area(int width, int height) { return width > 0 && height > 0; }

}  // namespace

Renderer::Renderer(const Window&) : impl_(std::make_unique<Impl>()) {
  impl_->load_pipeline();
  impl_->load_ui_font();
}

Renderer::~Renderer() { impl_->release(); }

void Renderer::upload(const SceneDesc& desc) {
  impl_->release_assets();
  impl_->faces_captured = 0;  // do not reflect the previous scene while the new one is captured
  ++impl_->scene_uploads;     // nor mix its frames into the new one's
  sync(desc);
}

void Renderer::sync(const SceneDesc& desc) {
  impl_->upload_new_textures(desc);
  impl_->upload_new_materials(desc);
  impl_->refresh_material_values(desc);
  impl_->upload_new_meshes(desc);
  impl_->note_see_through_meshes();
}

void Renderer::refresh_mesh(const SceneDesc& desc, int mesh) {
  const bool uploaded = is_index_of(mesh, impl_->meshes.size()) && is_index_of(mesh, desc.meshes.size());
  if (!uploaded) return;
  const auto index = static_cast<std::size_t>(mesh);
  const MeshDesc& source = desc.meshes[index];
  // The mesh is cut as it was when it was uploaded: each part gets the piece it was made from.
  const std::vector<PrimitiveDesc> pieces = mesh_pieces(source, config::kMaxVerticesPerMesh);
  for (const GpuPrimitive& part : impl_->meshes[index]) {
    if (part.piece < pieces.size()) move_vertices(part.mesh, pieces[part.piece]);
  }
  impl_->mesh_bounds[index] = source.bounds;
}

Camera3D Renderer::scene_camera(const Scene& scene) const { return impl_->camera_for(scene); }

void Renderer::draw_world(const Scene& scene, const Camera3D& camera, const RenderTexture2D* output,
                          const ParticleSystem* particles) {
  const int width = output != nullptr ? output->texture.width : GetRenderWidth();
  const int height = output != nullptr ? output->texture.height : GetRenderHeight();
  if (!has_area(width, height)) return;
  const Environment env = scene_environment(scene);
  impl_->ensure_targets(width, height, env.msaa);
  impl_->prepare_frame(scene, camera, env, static_cast<float>(width) / static_cast<float>(height));
  impl_->draw_lit_scene(camera, particles);
  impl_->post_process(env, output);
  impl_->end_temporal();
}

void Renderer::set_motion_blur(float amount) {
  impl_->motion_blur = std::clamp(amount, kNoMotionBlur, kFullMotionBlur);
}

void Renderer::allow_temporal_antialias(bool allowed) { impl_->temporal_allowed = allowed; }

void Renderer::set_clock(std::optional<double> seconds) { impl_->clock = seconds; }

void Renderer::set_images_dir(const std::filesystem::path& dir) {
  impl_->release_ui_images();
  impl_->images_dir = dir;
}

void Renderer::set_show_prefabs(bool show) { impl_->show_prefabs = show; }

}  // namespace redgem
