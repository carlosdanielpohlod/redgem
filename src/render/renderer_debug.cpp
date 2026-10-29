#include "render/renderer.hpp"

#include <rlgl.h>

#include "render/debug_draw.hpp"
#include "render/render_math.hpp"

namespace redgem {
namespace {
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
