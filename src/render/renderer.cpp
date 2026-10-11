#include "render/renderer.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/log.hpp"
#include "render/gpu_select.hpp"
#include "render/render_math.hpp"
#include "render/renderer_impl.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::to_raylib;
using render_math::to_vec3;

// On-screen touch controls: white, see-through, more solid under a finger.
constexpr unsigned char kTouchShade = config::kMaxColorByte;
constexpr unsigned char kTouchBaseAlpha = 28;
constexpr unsigned char kTouchRestingAlpha = 70;
constexpr unsigned char kTouchPressedAlpha = 150;
constexpr int kTraceLineLength = 512;     // characters of a raylib log line that are kept
constexpr float kUiLetterSpacing = 0.0f;  // extra space between letters, as a share of the text size
constexpr int kUiCornerSegments = 8;      // straight pieces each rounded corner is drawn with
constexpr float kNoMotionBlur = 0.0f;
constexpr float kFullMotionBlur = 1.0f;
// raylib's vertex buffer slots for a mesh's positions and normals.
constexpr int kPositionBuffer = 0;
constexpr int kNormalBuffer = 2;

// Passes on raylib's warnings and the one line that says which graphics card draws the game.
void report_graphics_card(int level, const char* format, va_list args) {
  char line[kTraceLineLength];
  std::vsnprintf(line, sizeof(line), format, args);
  const std::string text = line;
  if (const auto card = render_math::graphics_card_name(text)) {
    log::info("graphics card: " + *card);
  } else if (level >= LOG_WARNING) {
    log::warning(text);
  }
}

// Opens the window. raylib names the graphics card only at its most verbose level: listen while
// it opens.
void open_window(const RenderSettings& settings) {
  SetTraceLogCallback(report_graphics_card);
  SetTraceLogLevel(LOG_INFO);
  // No multisampling: the scene is drawn into off-screen targets and smoothed by the FXAA pass.
  unsigned int flags = FLAG_VSYNC_HINT;
  if (settings.resizable) flags |= FLAG_WINDOW_RESIZABLE;
  SetConfigFlags(flags);
  InitWindow(settings.width, settings.height, settings.title.c_str());
  SetTraceLogCallback(nullptr);
  SetTraceLogLevel(LOG_WARNING);
  if (!IsWindowReady()) throw std::runtime_error("cannot open the game window");
}

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

void draw_touch_circle(const TouchCircle& at, unsigned char resting, unsigned char pressed) {
  const ::Color fill{kTouchShade, kTouchShade, kTouchShade, at.pressed ? pressed : resting};
  DrawCircleV({at.x, at.y}, at.radius, fill);
}

// Writes what is on screen now to every path asked for.
void save_screenshots(const std::vector<std::filesystem::path>& paths) {
  rlDrawRenderBatchActive();
  Image frame = LoadImageFromScreen();
  for (const std::filesystem::path& path : paths) {
    std::filesystem::create_directories(path.parent_path());
    if (!ExportImage(frame, path.string().c_str())) log::error("cannot write screenshot " + path.string());
  }
  UnloadImage(frame);
}

}  // namespace

Renderer::Renderer(const RenderSettings& settings) : impl_(std::make_unique<Impl>()) {
  impl_->settings = settings;
  prefer_discrete_gpu();
  open_window(settings);
  SetExitKey(KEY_NULL);  // games decide when to quit
  SetTargetFPS(config::kTargetFps);
  impl_->load_pipeline();
  impl_->load_ui_font();
}

Renderer::~Renderer() {
  impl_->release();
  CloseWindow();
}

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
  if (mesh < 0 || static_cast<std::size_t>(mesh) >= impl_->meshes.size() ||
      static_cast<std::size_t>(mesh) >= desc.meshes.size()) {
    return;
  }
  const MeshDesc& source = desc.meshes[static_cast<std::size_t>(mesh)];
  std::vector<GpuPrimitive>& parts = impl_->meshes[static_cast<std::size_t>(mesh)];
  for (std::size_t i = 0; i < std::min(parts.size(), source.primitives.size()); ++i) {
    const PrimitiveDesc& primitive = source.primitives[i];
    // Same vertices in the same order, only moved: write over the buffers already on the card.
    if (static_cast<int>(primitive.positions.size() / config::kFloatsPerPosition) != parts[i].mesh.vertexCount)
      continue;
    const int bytes = static_cast<int>(primitive.positions.size() * sizeof(float));
    UpdateMeshBuffer(parts[i].mesh, kPositionBuffer, primitive.positions.data(), bytes, 0);
    UpdateMeshBuffer(parts[i].mesh, kNormalBuffer, primitive.normals.data(), bytes, 0);
  }
  impl_->mesh_bounds[static_cast<std::size_t>(mesh)] = source.bounds;
}

Camera3D Renderer::scene_camera(const Scene& scene) const { return impl_->camera_for(scene); }

void Renderer::draw_world(const Scene& scene, const Camera3D& camera, const RenderTexture2D* output,
                          const ParticleSystem* particles) {
  const int width = output != nullptr ? output->texture.width : GetRenderWidth();
  const int height = output != nullptr ? output->texture.height : GetRenderHeight();
  if (width <= 0 || height <= 0) return;  // a minimized window: nothing to draw into
  impl_->ensure_targets(width, height);
  const Environment env = scene_environment(scene);
  const Vec3 viewer = to_vec3(camera.position);
  impl_->place_objects(scene);
  impl_->apply_lights(scene, env, viewer);
  impl_->view_tan_half_fov = std::tan(camera.fovy * DEG2RAD * kHalf);
  impl_->view_aspect = static_cast<float>(width) / static_cast<float>(height);
  impl_->draw_shadow_map(scene, camera, env);
  impl_->capture_reflection_face(scene, camera);
  send(impl_->shader, impl_->shader.locs[SHADER_LOC_VECTOR_VIEW], camera.position);
  BeginTextureMode(impl_->hdr);
  ClearBackground(kBlack);
  BeginMode3D(camera);
  impl_->draw_sky(camera);
  impl_->draw_objects(scene, false, viewer);
  if (particles != nullptr) impl_->draw_particles(*particles, camera);
  EndMode3D();
  EndTextureMode();
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

int Renderer::screen_width() const { return GetRenderWidth(); }
int Renderer::screen_height() const { return GetRenderHeight(); }

void Renderer::set_cursor_locked(bool locked) {
  if (locked == IsCursorHidden()) return;
  if (locked) {
    DisableCursor();
  } else {
    EnableCursor();
  }
}

void Renderer::draw_touch_controls(const TouchLayout& controls) {
  draw_touch_circle(controls.stick_base, kTouchBaseAlpha, kTouchBaseAlpha);
  for (const TouchCircle* button : {&controls.stick_knob, &controls.a, &controls.b, &controls.start}) {
    draw_touch_circle(*button, kTouchRestingAlpha, kTouchPressedAlpha);
  }
}

void Renderer::draw(const Scene& scene, const std::vector<HudItem>& hud,
                    const std::vector<std::filesystem::path>& screenshot_paths, const ParticleSystem* particles,
                    const TouchLayout* touch) {
  BeginDrawing();
  draw_world(scene, impl_->camera_for(scene), nullptr, particles);
  draw_hud(hud);
  if (touch != nullptr) draw_touch_controls(*touch);
  if (impl_->settings.show_fps) DrawFPS(config::kFpsCounterX, config::kFpsCounterY);
  if (!screenshot_paths.empty()) save_screenshots(screenshot_paths);
  EndDrawing();
}

void Renderer::set_show_prefabs(bool show) { impl_->show_prefabs = show; }

bool Renderer::close_requested() const { return WindowShouldClose(); }

float Renderer::frame_seconds() const { return GetFrameTime(); }

}  // namespace redgem
