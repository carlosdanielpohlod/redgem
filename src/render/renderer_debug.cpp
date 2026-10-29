#include "render/renderer.hpp"

#include <rlgl.h>

#include "render/debug_draw.hpp"
#include "render/render_math.hpp"
#include "render/renderer_impl.hpp"
#include "core/log.hpp"
#include <utility>

namespace redgem {
namespace {
struct TemporalPause {
  bool& allowed;
  bool previous;
  explicit TemporalPause(bool& value) : allowed(value), previous(std::exchange(value, false)) {}
  ~TemporalPause() { allowed = previous; }
};
void release_surface(RenderTexture2D& target) {
  if (target.id != 0) UnloadRenderTexture(target);
  target = RenderTexture2D{};
}
bool load_surface(RenderTexture2D& target, int width, int height) {
  release_surface(target);
  target = LoadRenderTexture(width, height);
  if (target.id != 0 && rlFramebufferComplete(target.id)) return true;
  release_surface(target);
  return false;
}
void primitive(const DebugDrawItem& item) {
  const auto first = render_math::to_raylib(item.first);
  const auto second = render_math::to_raylib(item.second);
  const auto color = render_math::to_raylib(item.color);
  switch (item.kind) {
    case DebugDrawItem::Kind::Line:
      DrawLine3D(first, second, color);
      break;
    case DebugDrawItem::Kind::Box:
      DrawCubeWiresV(first, second, color);
      break;
  }
}
void draw_pass(const DebugDrawBuffer& commands, bool tested) {
  if (tested)
    rlEnableDepthTest();
  else
    rlDisableDepthTest();
  for (const auto& item : commands.items()) {
    if (item.depth_test == tested) primitive(item);
  }
  rlDrawRenderBatchActive();  // finish this pass before changing the depth test
}
}  // namespace
void Renderer::draw_debug(const DebugDrawBuffer& commands, const Camera3D& camera, const RenderTexture2D& output) {
  if (commands.items().empty()) return;
  BeginTextureMode(output);
  BeginMode3D(camera);
  rlDisableDepthMask();  // debug shapes never change the scene's depth or hide one another
  draw_pass(commands, true);
  draw_pass(commands, false);  // x-ray commands remain visible over depth-tested commands
  rlEnableDepthMask();
  EndMode3D();
  EndTextureMode();
}
}  // namespace redgem

namespace redgem {
RenderTexture2D* Renderer::Impl::debug_surface(int width, int height) {
  const auto size = std::make_pair(width, height);
  const bool invalid_size = width <= 0 || height <= 0;
  const bool refused = invalid_size || size == refused_debug_size;
  if (refused) return nullptr;
  const bool fitted =
      debug_output.id != 0 && debug_output.texture.width == width && debug_output.texture.height == height;
  if (fitted || load_surface(debug_output, width, height)) {
    refused_debug_size = {};
    return &debug_output;
  }
  refused_debug_size = size;
  log::warning("the graphics card cannot draw debug shapes at this window size; the game still draws");
  return nullptr;
}
void Renderer::draw_world(const Scene& scene, const Camera3D& camera, const RenderTexture2D* output,
                          const ParticleSystem* particles, const WaterRippleSnapshot* ripples,
                          const DebugDrawBuffer* commands) {
  const bool debug = commands != nullptr && !commands->items().empty();
  if (debug) return draw_debug_world(scene, camera, output, particles, ripples, *commands);
  draw_plain(scene, camera, output, particles, ripples);
}
void Renderer::draw_debug_world(const Scene& scene, const Camera3D& camera, const RenderTexture2D* output,
                                const ParticleSystem* particles, const WaterRippleSnapshot* ripples,
                                const DebugDrawBuffer& commands) {
  const auto* target = output != nullptr ? output : impl_->debug_surface(GetRenderWidth(), GetRenderHeight());
  if (target == nullptr) return draw_plain(scene, camera, output, particles, ripples);
  const TemporalPause pause(impl_->temporal_allowed);
  draw_plain(scene, camera, target, particles, ripples);
  draw_debug(commands, camera, *target);
  if (output == nullptr) render_detail::draw_full(*target, GetRenderWidth(), GetRenderHeight());
}
}  // namespace redgem
