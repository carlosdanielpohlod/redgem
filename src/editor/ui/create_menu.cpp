#include <imgui.h>

#include "core/config.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

// The shapes the 3D Object menu offers, each with its icon and the key of its label.
struct ShapeOption {
  PrimitiveShape shape;
  const char* icon;
  const char* key;
};
constexpr ShapeOption kShapeOptions[] = {
    {PrimitiveShape::Cube, ICON_FA_CUBE, "editor.create.shape.cube"},
    {PrimitiveShape::Sphere, ICON_FA_CIRCLE, "editor.create.shape.sphere"},
    {PrimitiveShape::Plane, ICON_FA_SQUARE, "editor.create.shape.plane"},
    {PrimitiveShape::Cylinder, ICON_FA_DATABASE, "editor.create.shape.cylinder"},
    {PrimitiveShape::Capsule, ICON_FA_CAPSULES, "editor.create.shape.capsule"},
};

// The kinds of light the Light menu offers.
struct LightOption {
  LightType type;
  const char* icon;
  const char* key;
};
constexpr LightOption kLightOptions[] = {
    {LightType::Directional, ICON_FA_SUN, "editor.create.light.directional"},
    {LightType::Point, ICON_FA_LIGHTBULB, "editor.create.light.point"},
    {LightType::Spot, ICON_FA_LIGHTBULB, "editor.create.light.spot"},
};

void draw_shape_items(EditorSession& session, Vec3 at) {
  for (const ShapeOption& option : kShapeOptions) {
    if (ImGui::MenuItem(icon_label(option.icon, option.key))) session.create_primitive(option.shape, at);
  }
}

void draw_light_items(EditorSession& session, Vec3 at) {
  for (const LightOption& option : kLightOptions) {
    if (ImGui::MenuItem(icon_label(option.icon, option.key))) session.create_light(option.type, at);
  }
}

// What a scene has one of, or a few: a camera, ground, the sky.
void draw_scenery_items(PanelContext& context) {
  EditorSession& session = context.session;
  const Vec3 at = context.spawn_point;
  if (ImGui::MenuItem(icon_label(ICON_FA_VIDEO, "editor.create.camera"))) session.create_camera(at);
  if (ImGui::MenuItem(icon_label(ICON_FA_MOUNTAIN, "editor.create.terrain"))) create_terrain(context);
  tooltip(tr("editor.create.terrain_tooltip"));
  if (ImGui::MenuItem(icon_label(ICON_FA_CLOUD_SUN, "editor.create.environment"))) session.create_environment();
  tooltip(tr("editor.create.environment_tooltip"));
}

void open_models_folder(const EditorSession& session) {
  const std::filesystem::path folder = session.game_dir() / config::kModelsDirName;
  std::error_code error;
  std::filesystem::create_directories(folder, error);
  platform::open_in_system(folder);
}

// The game's models, and how more of them get there.
void draw_model_items(PanelContext& context) {
  for (const std::string& model : context.files.models) {
    if (ImGui::MenuItem(model.c_str())) import_model(context.session, model, context.spawn_point);
  }
  if (!context.files.models.empty()) ImGui::Separator();
  ImGui::TextColored(muted_color(), "%s", tr("editor.create.models_hint"));
  if (ImGui::MenuItem(icon_label(ICON_FA_FOLDER_OPEN, "editor.create.open_models_folder"))) {
    open_models_folder(context.session);
  }
}

// A terrain was made, and the editor has a brush to turn on.
bool brush_ready(const PanelContext& context, ObjectId terrain) {
  return terrain != kNoObject && context.terrain != nullptr;
}

}  // namespace

ObjectId create_terrain(PanelContext& context) {
  const ObjectId terrain = context.session.create_terrain(context.spawn_point);
  // Dragging over the new ground shapes it right away.
  if (brush_ready(context, terrain)) context.terrain->sculpting = true;
  return terrain;
}

ObjectId import_model(EditorSession& session, const std::string& model_file, Vec3 position, ObjectId parent) {
  std::string error;
  const std::filesystem::path path = session.game_dir() / config::kModelsDirName / model_file;
  const ObjectId imported = session.import_model(path, position, error, parent);
  if (imported == kNoObject) log::error("cannot import " + model_file + ": " + error);
  return imported;
}

void draw_create_menu(PanelContext& context) {
  EditorSession& session = context.session;
  const Vec3 at = context.spawn_point;
  if (ImGui::MenuItem(icon_label(ICON_FA_CIRCLE_DOT, "editor.create.empty"))) session.create_empty(at);
  menu(icon_label(ICON_FA_CUBE, "editor.create.shape_menu"), [&session, at] { draw_shape_items(session, at); });
  menu(icon_label(ICON_FA_LIGHTBULB, "editor.create.light_menu"), [&session, at] { draw_light_items(session, at); });
  draw_scenery_items(context);
  ImGui::Separator();
  menu(icon_label(ICON_FA_FILE_IMPORT, "editor.create.model_menu"), [&context] { draw_model_items(context); });
}

}  // namespace redgem::ui
