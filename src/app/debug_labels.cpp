#include "app/debug_labels.hpp"

#include <cmath>

#include "core/math.hpp"
#include "script/script_context.hpp"

namespace redgem {
namespace {

int centered(float middle, int extent) {
  return static_cast<int>(std::lround(middle - static_cast<float>(extent) * kHalf));
}

int text_width_of(const ScriptContext& ctx, const DebugLabel& label) {
  if (ctx.measure_text) return ctx.measure_text(label.text, label.size);
  return estimated_text_width(label.text, label.size);
}

// The scene's main camera object, or nothing without one.
const GameObject* main_camera_object(const Scene* scene) {
  if (scene == nullptr) return nullptr;
  const ObjectId id = scene->main_camera();
  return id != kNoObject ? &scene->at(id) : nullptr;
}

// What the main camera shows, as the renderer draws it (its shake on it), or nothing without one.
std::optional<LabelView> main_view(const ScriptContext& ctx) {
  const GameObject* camera = main_camera_object(ctx.scene);
  if (camera == nullptr) return std::nullopt;
  const std::optional<CameraDesc>& lens = camera->camera;
  if (!lens) return std::nullopt;
  const Transform seen = viewed_from(ctx.scene->world_transform(ctx.scene->main_camera()), camera->camera_shake);
  const ScreenFrame frame{ctx.screen_width, ctx.screen_height, ctx.picture_width, ctx.picture_height};
  return LabelView{seen, Lens{lens->fov_y_radians, 0.0f}, frame};
}

}  // namespace

std::optional<HudItem> debug_label_item(const DebugLabel& label, const LabelView& view, int width) {
  const std::optional<ScreenPoint> point = world_to_screen(view.camera, view.lens, view.frame, label.at);
  if (!point) return std::nullopt;
  HudItem item;
  item.text = label.text;
  item.x = centered(point->x, width);
  item.y = centered(point->y, label.size);
  item.size = label.size;
  item.color = label.color;
  return item;
}

void append_debug_labels(ScriptContext& ctx, const DebugDrawBuffer& commands) {
  if (commands.labels().empty()) return;
  const std::optional<LabelView> view = main_view(ctx);
  if (!view) return;
  for (const DebugLabel& label : commands.labels()) {
    if (auto item = debug_label_item(label, *view, text_width_of(ctx, label))) ctx.hud.push_back(std::move(*item));
  }
}

}  // namespace redgem
