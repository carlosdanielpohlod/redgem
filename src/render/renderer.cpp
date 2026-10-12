#include "render/renderer.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "render/render_config.hpp"
#include "scene/scene_config.hpp"
#include "render/render_math.hpp"
#include "render/renderer_impl.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::to_raylib;

// On-screen touch controls: white, see-through, more solid under a finger.
constexpr unsigned char kTouchShade = config::kMaxColorByte;
constexpr unsigned char kTouchBaseAlpha = 28;
constexpr unsigned char kTouchRestingAlpha = 70;
constexpr unsigned char kTouchPressedAlpha = 150;
constexpr float kUiLetterSpacing = 0.0f;  // extra space between letters, as a share of the text size
constexpr int kUiCornerSegments = 8;      // straight pieces each rounded corner is drawn with
constexpr float kNoMotionBlur = 0.0f;
constexpr float kFullMotionBlur = 1.0f;
// raylib's vertex buffer slots for a mesh's positions and normals.
constexpr int kPositionBuffer = 0;
constexpr int kNormalBuffer = 2;

Rectangle box_of(const HudItem& item) {
  return {static_cast<float>(item.x), static_cast<float>(item.y), static_cast<float>(item.width),
          static_cast<float>(item.height)};
}

void draw_hud_rect(const HudItem& item) {
  const Rectangle box = box_of(item);
  const float roundness = render_math::corner_roundness(item.radius, box.width, box.height);
  if (roundness > render_math::kSquareCorners) {
    DrawRectangleRounded(box, roundness, kUiCornerSegments, to_raylib(item.color));
  } else {
    DrawRectangleRec(box, to_raylib(item.color));
  }
}

unsigned char control_alpha(ControlCircle::Look look) {
  if (look == ControlCircle::Look::Base) return kTouchBaseAlpha;
  return look == ControlCircle::Look::Pressed ? kTouchPressedAlpha : kTouchRestingAlpha;
}

// Same vertices in the same order, only moved: write over the buffers already on the card.
void move_vertices(const Mesh& mesh, const PrimitiveDesc& primitive) {
  if (static_cast<int>(primitive.positions.size() / config::kFloatsPerPosition) != mesh.vertexCount) return;
  const int bytes = static_cast<int>(primitive.positions.size() * sizeof(float));
  UpdateMeshBuffer(mesh, kPositionBuffer, primitive.positions.data(), bytes, 0);
  UpdateMeshBuffer(mesh, kNormalBuffer, primitive.normals.data(), bytes, 0);
}

}  // namespace

Renderer::Renderer(const Window&) : impl_(std::make_unique<Impl>()) {
  impl_->load_pipeline();
  impl_->load_ui_font();
}

Renderer::~Renderer() { impl_->release(); }

void Renderer::upload(const SceneDesc& desc) {
  impl_->release_assets();
  impl_->faces_captured = 0;  // do not reflect the previous scene while the new one is captured
  sync(desc);
}

void Renderer::sync(const SceneDesc& desc) {
  impl_->upload_new_textures(desc);
  impl_->upload_new_materials(desc);
  impl_->refresh_material_values(desc);
  impl_->upload_new_meshes(desc);
}

void Renderer::refresh_mesh(const SceneDesc& desc, int mesh) {
  const auto index = static_cast<std::size_t>(mesh);
  if (mesh < 0 || index >= impl_->meshes.size() || index >= desc.meshes.size()) return;
  const MeshDesc& source = desc.meshes[index];
  const std::vector<GpuPrimitive>& parts = impl_->meshes[index];
  for (std::size_t i = 0; i < std::min(parts.size(), source.primitives.size()); ++i) {
    move_vertices(parts[i].mesh, source.primitives[i]);
  }
  impl_->mesh_bounds[index] = source.bounds;
}

Camera3D Renderer::scene_camera(const Scene& scene) const { return impl_->camera_for(scene); }

void Renderer::draw_world(const Scene& scene, const Camera3D& camera, const RenderTexture2D* output,
                          const ParticleSystem* particles) {
  const int width = output != nullptr ? output->texture.width : GetRenderWidth();
  const int height = output != nullptr ? output->texture.height : GetRenderHeight();
  if (width <= 0 || height <= 0) return;  // a minimized window: nothing to draw into
  impl_->ensure_targets(width, height);
  const Environment env = scene_environment(scene);
  impl_->prepare_frame(scene, camera, env, static_cast<float>(width) / static_cast<float>(height));
  impl_->draw_lit_scene(scene, camera, particles);
  impl_->post_process(env, output);
}

void Renderer::Impl::draw_hud_text(const HudItem& item) {
  const float size = static_cast<float>(item.size);
  DrawTextEx(ui_font, item.text.c_str(), {static_cast<float>(item.x), static_cast<float>(item.y)}, size,
             size * kUiLetterSpacing, to_raylib(item.color));
}

void Renderer::Impl::draw_hud_image(const HudItem& item) {
  const Texture2D* picture = ui_image(item.text);
  if (picture == nullptr) return;
  const render_math::Size size = render_math::image_size(item.width, item.height, picture->width, picture->height);
  DrawTexturePro(*picture, {0.0f, 0.0f, static_cast<float>(picture->width), static_cast<float>(picture->height)},
                 {static_cast<float>(item.x), static_cast<float>(item.y), size.width, size.height}, {0.0f, 0.0f}, 0.0f,
                 to_raylib(item.color));
}

void Renderer::draw_hud(const std::vector<HudItem>& hud) {
  for (const HudItem& item : hud) {
    switch (item.kind) {
      case HudItem::Kind::Text:
        impl_->draw_hud_text(item);
        break;
      case HudItem::Kind::Rect:
        draw_hud_rect(item);
        break;
      case HudItem::Kind::Image:
        impl_->draw_hud_image(item);
        break;
    }
  }
}

void Renderer::set_motion_blur(float amount) {
  impl_->motion_blur = std::clamp(amount, kNoMotionBlur, kFullMotionBlur);
}

void Renderer::set_images_dir(const std::filesystem::path& dir) {
  impl_->release_ui_images();
  impl_->images_dir = dir;
}

int Renderer::text_width(const std::string& text, int size) const {
  const float text_size = static_cast<float>(size);
  return static_cast<int>(
      std::ceil(MeasureTextEx(impl_->ui_font, text.c_str(), text_size, text_size * kUiLetterSpacing).x));
}

void Renderer::draw_controls(const std::vector<ControlCircle>& controls) {
  for (const ControlCircle& circle : controls) {
    const ::Color fill{kTouchShade, kTouchShade, kTouchShade, control_alpha(circle.look)};
    DrawCircleV({circle.x, circle.y}, circle.radius, fill);
  }
}

void Renderer::set_show_prefabs(bool show) { impl_->show_prefabs = show; }

}  // namespace redgem
