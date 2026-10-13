#include <imgui.h>

#include "core/config.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

const char* shape_icon(PrimitiveShape shape) {
  switch (shape) {
    case PrimitiveShape::Sphere:
      return ICON_FA_CIRCLE;
    case PrimitiveShape::Plane:
      return ICON_FA_SQUARE;
    case PrimitiveShape::Cylinder:
      return ICON_FA_DATABASE;
    case PrimitiveShape::Capsule:
      return ICON_FA_CAPSULES;
    case PrimitiveShape::Cube:
      break;
  }
  return ICON_FA_CUBE;
}

// The kinds of light the Light menu offers.
struct LightOption {
  LightType type;
  const char* label;
};
constexpr const char* kHowToAddModels = "Drop .glb files (exported from Blender) on this window";

constexpr LightOption kLightOptions[] = {
    {LightType::Directional, ICON_FA_SUN "  Directional (sun)"},
    {LightType::Point, ICON_FA_LIGHTBULB "  Point"},
    {LightType::Spot, ICON_FA_LIGHTBULB "  Spot"},
};

void draw_shape_items(EditorSession& session, Vec3 at) {
  for (const PrimitiveShape shape : all_primitives()) {
    const std::string label = std::string(shape_icon(shape)) + "  " + primitive_name(shape);
    if (ImGui::MenuItem(label.c_str())) session.create_primitive(shape, at);
  }
}

void draw_light_items(EditorSession& session, Vec3 at) {
  for (const LightOption& option : kLightOptions) {
    if (ImGui::MenuItem(option.label)) session.create_light(option.type, at);
  }
}

// What a scene has one of, or a few: a camera, ground, the sky.
void draw_scenery_items(PanelContext& context) {
  EditorSession& session = context.session;
  const Vec3 at = context.spawn_point;
  if (ImGui::MenuItem(ICON_FA_VIDEO "  Camera")) session.create_camera(at);
  if (ImGui::MenuItem(ICON_FA_MOUNTAIN "  Terrain")) create_terrain(context);
  tooltip("Ground you shape with a brush: mountains, holes, plateaus.");
  if (ImGui::MenuItem(ICON_FA_CLOUD_SUN "  Environment")) session.create_environment();
  tooltip("Sky, fog, bloom and wind of the scene, as properties (colors are #rrggbb).");
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
  ImGui::TextColored(muted_color(), "%s", kHowToAddModels);
  if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Open the models folder")) open_models_folder(context.session);
}

}  // namespace

ObjectId create_terrain(PanelContext& context) {
  const ObjectId terrain = context.session.create_terrain(context.spawn_point);
  // Dragging over the new ground shapes it right away.
  if (terrain != kNoObject && context.terrain != nullptr) context.terrain->sculpting = true;
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
  if (ImGui::MenuItem(ICON_FA_CIRCLE_DOT "  Empty")) session.create_empty(at);
  menu(ICON_FA_CUBE "  3D Object", [&session, at] { draw_shape_items(session, at); });
  menu(ICON_FA_LIGHTBULB "  Light", [&session, at] { draw_light_items(session, at); });
  draw_scenery_items(context);
  ImGui::Separator();
  menu(ICON_FA_FILE_IMPORT "  Model", [&context] { draw_model_items(context); });
}

}  // namespace redgem::ui
