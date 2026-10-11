#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include "core/config.hpp"
#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/script_files.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"
#include "physics/body_config.hpp"
#include "script/script_naming.hpp"

namespace redgem::ui {
namespace {

using namespace editor_config;

constexpr const char* kBodyTypes[] = {"static", "dynamic", "kinematic", "character", "vehicle"};
constexpr const char* kBodyDescriptions[] = {
    "Never moves (floors, walls)", "Falls and gets pushed (crates, balls)", "Moved by scripts, pushes others",
    "Walks and jumps (CharacterController)", "Drives on its wheel children"};
constexpr const char* kColliders[] = {"box", "sphere", "capsule", "mesh"};
constexpr int kDefaultBody = 0;      // "static": what an object with a collider and no body is
constexpr int kDefaultCollider = 0;  // "box"
constexpr const char* kVehicleBody = "vehicle";  // wheels are children of an object with this body
constexpr const char* kLightTypes[] = {"Directional", "Point", "Spot"};  // in LightType's order
constexpr const char* kPropertyTypes[] = {"Number", "Text", "Toggle"};
constexpr int kNumberType = 0;  // positions in kPropertyTypes
constexpr int kTextType = 1;
constexpr double kDefaultNumber = 0.0;  // what a new Number property starts as
constexpr float kUnitMin = 0.0f;        // metallic and roughness go from 0 to 1
constexpr float kUnitMax = 1.0f;
constexpr float kRadiansToDegrees = 57.29577951f;
constexpr float kAddComponentWidth = 0.7f;  // of the panel width
constexpr float kNoLowerLimit = 0.0f;  // intensities, ranges, clip distances and glow are never negative
constexpr float kPopupFieldWidth = 160.0f;  // pixels: text fields inside the Add Component menus
constexpr const char* kSculptModes[] = {"Raise", "Lower", "Smooth", "Flatten"};  // in SculptMode's order
constexpr float kMinBrushRadius = 1.0f;     // meters
constexpr float kMaxBrushRadius = 30.0f;
constexpr float kMinBrushStrength = 0.5f;   // meters per second (see TerrainTool::strength)
constexpr float kMaxBrushStrength = 20.0f;
constexpr const char* kMetersFormat = "%.1f m";  // how distances are shown in sliders and drags
constexpr const char* kAddComponentPopup = "add_component";

// Widget ids (ImGui tells widgets apart by them; "##" keeps them off the screen).
constexpr const char* kNameId = "##name";
constexpr const char* kTagId = "##tag";
constexpr const char* kPositionId = "##position";
constexpr const char* kRotationId = "##rotation";
constexpr const char* kScaleId = "##scale";
constexpr const char* kBaseColorId = "##base";
constexpr const char* kMetallicId = "##metallic";
constexpr const char* kRoughnessId = "##roughness";
constexpr const char* kEmissionId = "##emission";
constexpr const char* kTypeId = "##type";
constexpr const char* kColorId = "##color";
constexpr const char* kIntensityId = "##intensity";
constexpr const char* kRangeId = "##range";
constexpr const char* kFovId = "##fov";
constexpr const char* kNearId = "##near";
constexpr const char* kFarId = "##far";
constexpr const char* kSculptId = "##sculpt";
constexpr const char* kBrushModeId = "##mode";
constexpr const char* kBrushRadiusId = "##radius";
constexpr const char* kBrushStrengthId = "##strength";
constexpr const char* kTriggerId = "##trigger";
constexpr const char* kBodyId = "##body";
constexpr const char* kColliderId = "##collider";
constexpr const char* kMassId = "##mass";
constexpr const char* kWheelId = "##wheel";
constexpr const char* kValueId = "##value";
constexpr const char* kNewScriptId = "##new_script";
constexpr const char* kNewPropertyId = "##key";

// Properties edited by dedicated widgets instead of the generic list.
const std::set<std::string>& reserved_props() {
  static const std::set<std::string> keys = {
      config::kPropScript, config::kPropBody, config::kPropCollider, config::kPropTrigger, config::kPropMass,
      config::kPropTag,    config::kPropWheel, config::kPropVisible, config::kPropPrefab,
      config::kPropTerrain, config::kPropTerrainSize, config::kPropTerrainCells};
  return keys;
}

// Undo for widgets whose edits stream in (drags, sliders, color pickers): one undo step per
// interaction, taken just before its first change, and dropped if it ends where it started.
class EditStream {
 public:
  void before_change(SceneDocument& doc) {
    if (doc_ == &doc) return;
    doc.checkpoint();
    doc_ = &doc;
  }
  void end_frame() {
    if (doc_ == nullptr || ImGui::IsAnyItemActive()) return;
    doc_->drop_checkpoint_if_unchanged();
    doc_ = nullptr;
  }

 private:
  SceneDocument* doc_ = nullptr;  // the document of the interaction in progress
};
EditStream g_stream;

bool is_object(EditorSession& session, int index) { return index >= 0 && index < session.document().node_count(); }

// A text edit to apply: to `object`, the one the field showed when typing started (the selection
// may have changed since).
struct TextEdit {
  int object = kNoIndex;
  std::string text;
};

// What a text field is doing between frames.
struct TextFieldState {
  std::string buffer;
  std::string original;
  int object = kNoIndex;
  bool editing = false;
};
// State kept between frames. Reset when another game is opened (see reset_inspector_state).
std::map<ImGuiID, TextFieldState> g_text_fields;
std::string g_new_script_name;
std::string g_new_property_key;
int g_new_property_type = kNumberType;

// Commits a text field when the user leaves it. Each field keeps its own buffer, so moving from one
// field to another never loses typing; clicking another object commits to the one being edited.
std::optional<TextEdit> text_field(const char* id, int object, const std::string& current, const char* hint = "") {
  TextFieldState& field = g_text_fields[ImGui::GetID(id)];
  if (!field.editing) field = {current, current, object, false};  // follow the document while idle
  ImGui::InputTextWithHint(id, hint, &field.buffer);
  if (ImGui::IsItemActive()) {
    field.editing = true;
    return std::nullopt;
  }
  if (!field.editing) return std::nullopt;
  field.editing = false;  // just left the field (Enter, Tab or a click elsewhere)
  if (field.buffer == field.original) return std::nullopt;
  return TextEdit{field.object, field.buffer};
}

void set_or_remove(EditorSession& session, int index, const char* key, const std::optional<PropertyValue>& value) {
  if (value) {
    session.document().set_prop(index, key, *value);
  } else {
    session.document().remove_prop(index, key);
  }
  session.refresh_preview();
}

// A flag property that is only stored when it differs from its default.
std::optional<PropertyValue> flag_or_nothing(bool stored, bool value) {
  return stored ? std::optional<PropertyValue>(value) : std::nullopt;
}

// The scripts attached to an object, in order.
std::vector<std::string> scripts_of(EditorSession& session, int index) {
  return split_script_list(get_string(session.document().node(index).props, config::kPropScript).value_or(""));
}

const char* object_icon(const NodeDesc& node) {
  if (node.camera) return ICON_FA_VIDEO;
  if (node.light) return node.light->type == LightType::Directional ? ICON_FA_SUN : ICON_FA_LIGHTBULB;
  if (node.mesh != kNoIndex) return ICON_FA_CUBE;
  return ICON_FA_CIRCLE_DOT;
}

// --- Header ------------------------------------------------------------------------------------

void draw_header(EditorSession& session, int index) {
  const NodeDesc& node = session.document().node(index);
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(accent_color(), "%s", object_icon(node));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kFillWidth);
  if (auto name = text_field(kNameId, index, node.name); name && is_object(session, name->object)) {
    if (!name->text.empty()) session.document().set_name(name->object, session.document().unique_name(name->text));
    session.refresh_preview();
  }
  const PropertyMap& props = session.document().node(index).props;
  property_label("Tag");
  if (auto tag = text_field(kTagId, index, get_string(props, config::kPropTag).value_or(""), "untagged");
      tag && is_object(session, tag->object)) {
    set_or_remove(session, tag->object, config::kPropTag,
                  tag->text.empty() ? std::nullopt : std::optional<PropertyValue>(tag->text));
  }
  bool visible = get_bool(props, config::kPropVisible).value_or(true);
  if (ImGui::Checkbox("Visible", &visible)) {
    set_or_remove(session, index, config::kPropVisible, flag_or_nothing(!visible, false));
  }
  ImGui::SameLine();
  bool prefab = get_bool(props, config::kPropPrefab).value_or(false);
  if (ImGui::Checkbox("Template", &prefab)) {
    set_or_remove(session, index, config::kPropPrefab, flag_or_nothing(prefab, true));
  }
  tooltip("Templates are hidden in the game and copied with Scene.spawn(\"Name\", position: ...).");
}

// --- Transform ---------------------------------------------------------------------------------

void draw_transform(EditorSession& session, int index) {
  if (!component_header(ICON_FA_UP_DOWN_LEFT_RIGHT, "Transform")) return;
  SceneDocument& doc = session.document();
  Transform local = doc.node(index).local;
  float position[] = {local.position.x, local.position.y, local.position.z};
  const Vec3 euler = quat_to_euler_degrees(local.rotation);
  float rotation[] = {euler.x, euler.y, euler.z};
  float scale[] = {local.scale.x, local.scale.y, local.scale.z};

  property_label("Position");
  const bool moved = ImGui::DragFloat3(kPositionId, position, kDragSpeedPosition);
  property_label("Rotation");
  const bool rotated = ImGui::DragFloat3(kRotationId, rotation, kDragSpeedRotation);
  property_label("Scale");
  const bool scaled = ImGui::DragFloat3(kScaleId, scale, kDragSpeedScale, kMinScale, FLT_MAX);
  if (!moved && !rotated && !scaled) return;
  g_stream.before_change(doc);
  local.position = {position[0], position[1], position[2]};
  if (rotated) local.rotation = quat_from_euler_degrees({rotation[0], rotation[1], rotation[2]});
  // Only when the scale itself was edited: a mirrored object (negative scale) must survive a move.
  if (scaled) local.scale = {std::max(scale[0], kMinScale), std::max(scale[1], kMinScale), std::max(scale[2], kMinScale)};
  doc.set_local(index, local, false);
  session.refresh_preview();
}

// --- Mesh and materials ------------------------------------------------------------------------

void draw_material(EditorSession& session, int material) {
  SceneDocument& doc = session.document();
  MaterialDesc edited = doc.material(material);
  ImGui::PushID(material);
  const std::string title = edited.name.empty() ? "Material " + std::to_string(material) : edited.name;
  ImGui::SeparatorText(title.c_str());
  const int users = doc.material_users(material);
  if (users > 1) ImGui::TextColored(muted_color(), ICON_FA_LINK "  Shared by %d objects", users);

  float base[] = {edited.base_color.r, edited.base_color.g, edited.base_color.b, edited.base_color.a};
  float emission[] = {edited.emissive.x, edited.emissive.y, edited.emissive.z};
  bool changed = false;
  property_label(edited.base_color_image != kNoIndex ? "Tint" : "Color");
  changed |= ImGui::ColorEdit4(kBaseColorId, base, ImGuiColorEditFlags_Float);
  property_label("Metallic");
  changed |= ImGui::SliderFloat(kMetallicId, &edited.metallic, kUnitMin, kUnitMax);
  property_label("Roughness");
  changed |= ImGui::SliderFloat(kRoughnessId, &edited.roughness, kUnitMin, kUnitMax);
  property_label("Emission");
  changed |= ImGui::ColorEdit3(kEmissionId, emission, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
  tooltip("Glow color; values above 1 glow brighter.");
  if (edited.base_color_image != kNoIndex || edited.normal_image != kNoIndex) {
    ImGui::TextColored(muted_color(), ICON_FA_IMAGE "  Textured%s", edited.normal_image != kNoIndex ? " + normal map" : "");
  }
  if (changed) {
    g_stream.before_change(doc);
    edited.base_color = {base[0], base[1], base[2], base[3]};
    edited.emissive = {std::max(emission[0], kNoLowerLimit), std::max(emission[1], kNoLowerLimit),
                      std::max(emission[2], kNoLowerLimit)};
    doc.set_material(material, edited, false);
  }
  ImGui::PopID();
}

void draw_mesh(EditorSession& session, int index) {
  const NodeDesc& node = session.document().node(index);
  if (node.mesh == kNoIndex) return;
  if (!component_header(ICON_FA_CUBE, "Mesh")) return;
  const MeshDesc& mesh = session.scene_desc().meshes[node.mesh];
  property_label("Mesh");
  ImGui::TextUnformatted(mesh.name.empty() ? "(unnamed)" : mesh.name.c_str());
  for (const int material : editor_logic::materials_of(mesh)) draw_material(session, material);
}

// --- Light and camera --------------------------------------------------------------------------

void draw_light(EditorSession& session, int index) {
  const NodeDesc& node = session.document().node(index);
  if (!node.light) return;
  if (!component_header(ICON_FA_LIGHTBULB, "Light")) return;
  LightDesc light = *node.light;
  int type = static_cast<int>(light.type);
  float color[] = {light.color.r, light.color.g, light.color.b};
  bool changed = false;
  property_label("Type");
  changed |= ImGui::Combo(kTypeId, &type, kLightTypes, static_cast<int>(std::size(kLightTypes)));
  property_label("Color");
  changed |= ImGui::ColorEdit3(kColorId, color, ImGuiColorEditFlags_Float);
  property_label("Intensity");
  changed |= ImGui::DragFloat(kIntensityId, &light.intensity, kDragSpeedIntensity, kNoLowerLimit, FLT_MAX);
  tooltip(light.type == LightType::Directional ? "Sun strength" : "Watts (like Blender)");
  if (light.type != LightType::Directional) {
    property_label("Range");
    changed |= ImGui::DragFloat(kRangeId, &light.range, kDragSpeedNumber, kNoLowerLimit, FLT_MAX, kMetersFormat);
    tooltip("0 = not set (the engine then uses its default reach)");
  }
  if (!changed) return;
  g_stream.before_change(session.document());
  light.type = static_cast<LightType>(type);
  light.color = {color[0], color[1], color[2], light.color.a};
  light.intensity = std::max(light.intensity, kNoLowerLimit);
  light.range = std::max(light.range, kNoLowerLimit);
  session.document().set_light(index, light, false);
  session.refresh_preview();
}

void draw_camera(EditorSession& session, int index) {
  const NodeDesc& node = session.document().node(index);
  if (!node.camera) return;
  if (!component_header(ICON_FA_VIDEO, "Camera")) return;
  CameraDesc camera = *node.camera;
  float fov = camera.fov_y_radians * kRadiansToDegrees;
  bool changed = false;
  property_label("Field of view");
  changed |= ImGui::SliderFloat(kFovId, &fov, kMinFovDegrees, kMaxFovDegrees, "%.0f deg");
  property_label("Near");
  changed |= ImGui::DragFloat(kNearId, &camera.near_plane, kDragSpeedScale, kMinClipDistance, FLT_MAX);
  property_label("Far");
  changed |= ImGui::DragFloat(kFarId, &camera.far_plane, kDragSpeedNumber, kNoLowerLimit, FLT_MAX);
  tooltip("0 = infinite");
  if (session.scene().main_camera() == index) ImGui::TextColored(muted_color(), ICON_FA_STAR "  The game starts with this camera");
  if (!changed) return;
  g_stream.before_change(session.document());
  camera.fov_y_radians = std::clamp(fov, kMinFovDegrees, kMaxFovDegrees) / kRadiansToDegrees;
  camera.near_plane = std::max(camera.near_plane, kMinClipDistance);
  session.document().set_camera(index, camera, false);
  session.refresh_preview();
}

// --- Scripts -----------------------------------------------------------------------------------

void write_scripts(EditorSession& session, int index, const std::vector<std::string>& scripts) {
  const std::string joined = join_script_list(scripts);
  set_or_remove(session, index, config::kPropScript, joined.empty() ? std::nullopt : std::optional<PropertyValue>(joined));
}

// The card of one attached script: a button that opens it, or a warning when its file is gone.
void draw_script_card(PanelContext& context, const std::string& script) {
  property_label("Script");
  const std::vector<std::string>& known = context.files.scripts;
  if (std::find(known.begin(), known.end(), script) == known.end()) {
    ImGui::TextColored(error_color(), ICON_FA_TRIANGLE_EXCLAMATION "  %s is missing", script.c_str());
    return;
  }
  if (ImGui::Button((std::string(ICON_FA_PEN_TO_SQUARE "  ") + script).c_str())) open_script(context.scripts, script);
  tooltip("Edit in a script tab");
}

void draw_scripts(PanelContext& context, int index) {
  EditorSession& session = context.session;
  const std::vector<std::string> scripts = scripts_of(session, index);
  for (std::size_t i = 0; i < scripts.size(); ++i) {
    ImGui::PushID(static_cast<int>(i));
    bool removed = false;
    const std::string title = class_name_for_script(scripts[i]);
    const bool open = component_header(ICON_FA_GEM, title.c_str(), &removed);
    if (removed) {
      std::vector<std::string> remaining = scripts;
      remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
      write_scripts(session, index, remaining);
      ImGui::PopID();
      return;
    }
    if (open) draw_script_card(context, scripts[i]);
    ImGui::PopID();
  }
}

// --- Physics -----------------------------------------------------------------------------------

// One undo step for the whole component.
void remove_physics(EditorSession& session, int index) {
  SceneDocument& doc = session.document();
  doc.checkpoint();
  for (const char* key : {config::kPropBody, config::kPropCollider, config::kPropMass, config::kPropTrigger}) {
    if (doc.node(index).props.count(key) != 0) doc.remove_prop(index, key, false);
  }
  doc.drop_checkpoint_if_unchanged();
  session.refresh_preview();
}

// The brush that sculpts a terrain in the Scene view.
void draw_terrain(PanelContext& context, int index) {
  if (context.terrain == nullptr || !context.session.is_terrain(index)) return;
  if (!component_header(ICON_FA_MOUNTAIN, "Terrain")) return;
  TerrainTool& tool = *context.terrain;
  property_label("Sculpt");
  ImGui::Checkbox(kSculptId, &tool.sculpting);
  tooltip("While on, drag the left mouse button over the terrain in the Scene view. Shift reverses Raise and Lower.");
  property_label("Brush");
  int mode = static_cast<int>(tool.mode);
  ImGui::SetNextItemWidth(kFillWidth);
  if (ImGui::Combo(kBrushModeId, &mode, kSculptModes, static_cast<int>(std::size(kSculptModes)))) {
    tool.mode = static_cast<SculptMode>(mode);
  }
  property_label("Radius");
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::SliderFloat(kBrushRadiusId, &tool.radius, kMinBrushRadius, kMaxBrushRadius, kMetersFormat);
  property_label("Strength");
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::SliderFloat(kBrushStrengthId, &tool.strength, kMinBrushStrength, kMaxBrushStrength, "%.1f");
}

// Each of the rows below returns true when it changed the object: the card is then drawn again
// from the new properties on the next frame.
bool draw_trigger_row(EditorSession& session, int index, bool& trigger) {
  property_label("Trigger");
  if (ImGui::Checkbox(kTriggerId, &trigger)) {
    set_or_remove(session, index, config::kPropTrigger, flag_or_nothing(trigger, true));
    return true;
  }
  tooltip("Detects overlaps (on_trigger_enter) instead of colliding.");
  return false;
}

bool draw_body_row(EditorSession& session, int index, const PropertyMap& props) {
  const int count = static_cast<int>(std::size(kBodyTypes));
  const std::string current = get_string(props, config::kPropBody).value_or(kBodyTypes[kDefaultBody]);
  int type = editor_logic::index_of(kBodyTypes, count, current, kDefaultBody);
  property_label("Body");
  if (ImGui::Combo(kBodyId, &type, kBodyTypes, count)) {
    set_or_remove(session, index, config::kPropBody, PropertyValue(std::string(kBodyTypes[type])));
    return true;
  }
  tooltip(kBodyDescriptions[type]);
  return false;
}

bool draw_collider_row(EditorSession& session, int index, const PropertyMap& props) {
  const int count = static_cast<int>(std::size(kColliders));
  const std::string current = get_string(props, config::kPropCollider).value_or(kColliders[kDefaultCollider]);
  int collider = editor_logic::index_of(kColliders, count, current, kDefaultCollider);
  property_label("Collider");
  if (!ImGui::Combo(kColliderId, &collider, kColliders, count)) return false;
  set_or_remove(session, index, config::kPropCollider, PropertyValue(std::string(kColliders[collider])));
  return true;
}

void draw_mass_row(EditorSession& session, int index, float mass) {
  property_label("Mass");
  if (!ImGui::DragFloat(kMassId, &mass, kDragSpeedNumber, kMinMass, FLT_MAX, "%.2f kg")) return;
  g_stream.before_change(session.document());
  session.document().set_prop(index, config::kPropMass, static_cast<double>(std::max(mass, kMinMass)), false);
}

void draw_physics(EditorSession& session, int index) {
  const PropertyMap& props = session.document().node(index).props;
  const BodyConfig body = body_config_from_props(props);
  const bool has_collider = props.count(config::kPropCollider) != 0;
  if (!body.wants_physics() && !has_collider) return;
  bool removed = false;
  const bool open = component_header(ICON_FA_WEIGHT_HANGING, "Physics", &removed);
  if (removed) {
    remove_physics(session, index);
    return;
  }
  if (!open) return;

  bool trigger = get_bool(props, config::kPropTrigger).value_or(false);
  if (draw_trigger_row(session, index, trigger)) return;
  if (!trigger && draw_body_row(session, index, props)) return;
  if (draw_collider_row(session, index, props)) return;
  if (body.type == BodyType::Dynamic || body.type == BodyType::Vehicle) draw_mass_row(session, index, body.mass);
  for (const std::string& warning : body.warnings) {
    ImGui::TextColored(warning_color(), ICON_FA_TRIANGLE_EXCLAMATION "  %s", warning.c_str());
  }
}

// Wheels are children of a vehicle: shown on the child itself.
void draw_wheel(EditorSession& session, int index) {
  const NodeDesc& node = session.document().node(index);
  if (node.parent == kNoIndex) return;
  const auto parent_body = get_string(session.document().node(node.parent).props, config::kPropBody);
  const bool is_wheel = get_bool(node.props, config::kPropWheel).value_or(false);
  if (parent_body != std::optional<std::string>(kVehicleBody) && !is_wheel) return;
  if (!component_header(ICON_FA_CAR, "Vehicle wheel")) return;
  bool wheel = is_wheel;
  property_label("Is a wheel");
  if (ImGui::Checkbox(kWheelId, &wheel)) set_or_remove(session, index, config::kPropWheel, flag_or_nothing(wheel, true));
}

// --- Custom properties -------------------------------------------------------------------------

bool is_custom(const std::string& key) { return reserved_props().count(key) == 0; }

// The widget of one custom property, by what it holds: a checkbox, a number or a text field.
void draw_custom_value(EditorSession& session, int index, const std::string& key, const PropertyValue& value) {
  if (const auto* flag = std::get_if<bool>(&value)) {
    bool checked = *flag;
    if (ImGui::Checkbox(kValueId, &checked)) set_or_remove(session, index, key.c_str(), PropertyValue(checked));
    return;
  }
  if (const auto* number = std::get_if<double>(&value)) {
    double edited = *number;
    if (!ImGui::DragScalar(kValueId, ImGuiDataType_Double, &edited, kDragSpeedNumber)) return;
    g_stream.before_change(session.document());
    session.document().set_prop(index, key, edited, false);
    return;
  }
  if (auto text = text_field(kValueId, index, std::get<std::string>(value)); text && is_object(session, text->object)) {
    set_or_remove(session, text->object, key.c_str(), PropertyValue(text->text));
  }
}

void draw_custom_props(EditorSession& session, int index) {
  const PropertyMap props = session.document().node(index).props;  // copy: edits change the map
  const bool any = std::any_of(props.begin(), props.end(), [](const auto& entry) { return is_custom(entry.first); });
  if (!any) return;
  if (!component_header(ICON_FA_SLIDERS, "Properties")) return;
  for (const auto& [key, value] : props) {
    if (!is_custom(key)) continue;
    ImGui::PushID(key.c_str());
    if (ImGui::SmallButton(ICON_FA_XMARK)) {
      set_or_remove(session, index, key.c_str(), std::nullopt);
      ImGui::PopID();
      continue;
    }
    tooltip("Remove");
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(muted_color(), "%s", key.c_str());
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kFillWidth);
    draw_custom_value(session, index, key, value);
    ImGui::PopID();
  }
  ImGui::TextColored(muted_color(), "Read them in Ruby with prop(:name).");
}

// --- Add Component -----------------------------------------------------------------------------

// Makes scripts/<name>.rb, attaches it to the object and opens it in a tab.
void create_and_attach_script(PanelContext& context, int index, std::string& name) {
  std::string error;
  const auto file = create_script(context.scripts.scripts_dir(), name, error);
  if (!file) {
    log::error(error);
    return;
  }
  context.session.attach_script(index, *file);
  open_script(context.scripts, *file);
  context.files_changed = true;
  name.clear();
  log::info("created " + *file);
  ImGui::CloseCurrentPopup();
}

void add_script_menu(PanelContext& context, int index) {
  EditorSession& session = context.session;
  const std::vector<std::string> attached = scripts_of(session, index);
  std::string& new_name = g_new_script_name;
  ImGui::SetNextItemWidth(kPopupFieldWidth);
  const bool entered =
      ImGui::InputTextWithHint(kNewScriptId, "new_script_name", &new_name, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  const bool valid = valid_script_name(new_name);
  ImGui::BeginDisabled(!valid);
  if (ImGui::Button(ICON_FA_PLUS " New") || (entered && valid)) create_and_attach_script(context, index, new_name);
  ImGui::EndDisabled();
  if (!new_name.empty() && !valid) ImGui::TextColored(muted_color(), "%s", kScriptNameRule);
  if (!context.files.scripts.empty()) ImGui::Separator();
  for (const std::string& script : context.files.scripts) {
    if (std::find(attached.begin(), attached.end(), script) != attached.end()) continue;
    if (ImGui::MenuItem((std::string(ICON_FA_GEM "  ") + script).c_str())) session.attach_script(index, script);
  }
}

// What a new property of each kind starts as.
PropertyValue initial_value(int type) {
  if (type == kNumberType) return PropertyValue(kDefaultNumber);
  return type == kTextType ? PropertyValue(std::string()) : PropertyValue(false);
}

void add_property_menu(EditorSession& session, int index) {
  std::string& key = g_new_property_key;
  int& type = g_new_property_type;
  ImGui::SetNextItemWidth(kPopupFieldWidth);
  ImGui::InputTextWithHint(kNewPropertyId, "name", &key);
  ImGui::SetNextItemWidth(kPopupFieldWidth);
  ImGui::Combo(kTypeId, &type, kPropertyTypes, static_cast<int>(std::size(kPropertyTypes)));
  const PropertyMap& props = session.document().node(index).props;
  const bool valid = !key.empty() && reserved_props().count(key) == 0 && props.count(key) == 0;
  ImGui::BeginDisabled(!valid);
  if (ImGui::Button(ICON_FA_PLUS " Add property")) {
    set_or_remove(session, index, key.c_str(), initial_value(type));
    key.clear();
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
}

void add_body_menu(EditorSession& session, int index) {
  for (int i = 0; i < static_cast<int>(std::size(kBodyTypes)); ++i) {
    if (ImGui::MenuItem(kBodyTypes[i])) {
      set_or_remove(session, index, config::kPropBody, PropertyValue(std::string(kBodyTypes[i])));
    }
    tooltip(kBodyDescriptions[i]);
  }
}

void draw_add_component(PanelContext& context, int index) {
  EditorSession& session = context.session;
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  const float available = ImGui::GetContentRegionAvail().x;
  const float width = available * kAddComponentWidth;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + editor_logic::centered_start(available, width));
  if (ImGui::Button(ICON_FA_PLUS "  Add Component", ImVec2(width, 0.0f))) ImGui::OpenPopup(kAddComponentPopup);
  if (!ImGui::BeginPopup(kAddComponentPopup)) return;
  if (ImGui::BeginMenu(ICON_FA_GEM "  Script")) {
    add_script_menu(context, index);
    ImGui::EndMenu();
  }
  const PropertyMap& props = session.document().node(index).props;
  const bool has_physics = body_config_from_props(props).wants_physics() || props.count(config::kPropCollider) != 0;
  if (ImGui::BeginMenu(ICON_FA_WEIGHT_HANGING "  Physics body", !has_physics)) {
    add_body_menu(session, index);
    ImGui::EndMenu();
  }
  if (ImGui::MenuItem(ICON_FA_BULLSEYE "  Trigger", nullptr, false, !has_physics)) {
    set_or_remove(session, index, config::kPropTrigger, PropertyValue(true));
  }
  if (ImGui::BeginMenu(ICON_FA_SLIDERS "  Property")) {
    add_property_menu(session, index);
    ImGui::EndMenu();
  }
  ImGui::EndPopup();
}

void draw_runtime_view(EditorSession& session, int index) {
  const Scene& scene = session.scene();
  if (!scene.is_alive(index)) {
    ImGui::TextColored(muted_color(), "Destroyed during play.");
    return;
  }
  const Transform world = scene.world_transform(index);
  const Vec3 euler = quat_to_euler_degrees(world.rotation);
  if (component_header(ICON_FA_UP_DOWN_LEFT_RIGHT, "Transform (live)")) {
    property_label("Position");
    ImGui::Text("%.2f  %.2f  %.2f", world.position.x, world.position.y, world.position.z);
    property_label("Rotation");
    ImGui::Text("%.1f  %.1f  %.1f", euler.x, euler.y, euler.z);
  }
  ImGui::Spacing();
  ImGui::TextColored(muted_color(), ICON_FA_CIRCLE_INFO "  Stop the game to edit.");
}

}  // namespace

void open_script(ScriptWorkspace& scripts, const std::string& file_name) {
  std::string error;
  if (scripts.open(file_name, error) == nullptr) log::error(error);
}

void draw_inspector(PanelContext& context) {
  EditorSession& session = context.session;
  ImGui::Begin(kInspectorWindow);
  const ObjectId index = session.selection();
  if (index == kNoObject) {
    ImGui::TextColored(muted_color(), ICON_FA_ARROW_POINTER "  Select an object in the Hierarchy or the Scene.");
    ImGui::End();
    return;
  }
  // Widget IDs do not include the object: a text field being edited when another object gets
  // selected still commits to the object it was showing (see text_field).
  if (session.play_state() != PlayState::Editing) {
    ImGui::TextUnformatted(session.document().node(index).name.c_str());
    draw_runtime_view(session, index);
  } else {
    draw_header(session, index);
    draw_transform(session, index);
    draw_mesh(session, index);
    draw_light(session, index);
    draw_camera(session, index);
    draw_terrain(context, index);
    draw_scripts(context, index);
    draw_physics(session, index);
    draw_wheel(session, index);
    draw_custom_props(session, index);
    draw_add_component(context, index);
  }
  g_stream.end_frame();
  ImGui::End();
}

void reset_inspector_state() {
  g_text_fields.clear();
  g_new_script_name.clear();
  g_new_property_key.clear();
  g_new_property_type = kNumberType;
  g_stream = EditStream{};  // it points at the previous game's document
}

void reset_panel_state() {
  reset_hierarchy_state();
  reset_inspector_state();
  reset_project_state();
  reset_script_tabs_state();
}

}  // namespace redgem::ui
