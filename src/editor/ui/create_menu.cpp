#include <imgui.h>

#include "core/config.hpp"
#include "core/log.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

const char* shape_icon(PrimitiveShape shape) {
  switch (shape) {
    case PrimitiveShape::Sphere: return ICON_FA_CIRCLE;
    case PrimitiveShape::Plane: return ICON_FA_SQUARE;
    case PrimitiveShape::Cylinder: return ICON_FA_DATABASE;
    case PrimitiveShape::Capsule: return ICON_FA_CAPSULES;
    case PrimitiveShape::Cube: break;
  }
  return ICON_FA_CUBE;
}

}  // namespace

void draw_create_menu(PanelContext& context) {
  EditorSession& session = context.session;
  const Vec3 at = context.spawn_point;
  if (ImGui::MenuItem(ICON_FA_CIRCLE_DOT "  Empty")) session.create_empty(at);
  if (ImGui::BeginMenu(ICON_FA_CUBE "  3D Object")) {
    for (const PrimitiveShape shape : all_primitives()) {
      const std::string label = std::string(shape_icon(shape)) + "  " + primitive_name(shape);
      if (ImGui::MenuItem(label.c_str())) session.create_primitive(shape, at);
    }
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu(ICON_FA_LIGHTBULB "  Light")) {
    if (ImGui::MenuItem(ICON_FA_SUN "  Directional (sun)")) session.create_light(LightType::Directional, at);
    if (ImGui::MenuItem(ICON_FA_LIGHTBULB "  Point")) session.create_light(LightType::Point, at);
    if (ImGui::MenuItem(ICON_FA_LIGHTBULB "  Spot")) session.create_light(LightType::Spot, at);
    ImGui::EndMenu();
  }
  if (ImGui::MenuItem(ICON_FA_VIDEO "  Camera")) session.create_camera(at);
  if (ImGui::MenuItem(ICON_FA_MOUNTAIN "  Terrain")) session.create_terrain(at);
  tooltip("Ground you can sculpt: select it and turn Sculpt on in the Inspector");
  if (ImGui::MenuItem(ICON_FA_CLOUD_SUN "  Environment")) session.create_environment();
  tooltip("Sky, fog, bloom and wind of the scene, as properties (colors are #rrggbb).");
  ImGui::Separator();
  if (ImGui::BeginMenu(ICON_FA_FILE_IMPORT "  Model from models/", !context.files.models.empty())) {
    for (const std::string& model : context.files.models) {
      if (!ImGui::MenuItem(model.c_str())) continue;
      std::string error;
      if (session.import_model(session.game_dir() / config::kModelsDirName / model, at, error) == kNoObject) {
        log::error("cannot import " + model + ": " + error);
      }
    }
    ImGui::EndMenu();
  }
  if (context.files.models.empty()) tooltip("Put .glb files (exported from Blender) in the game's models/ folder.");
}

}  // namespace redgem::ui
