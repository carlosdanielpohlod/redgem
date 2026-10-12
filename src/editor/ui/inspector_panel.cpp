#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <set>

#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/script_files.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"
#include "physics/body_config.hpp"
#include "scene/props.hpp"
#include "script/script_naming.hpp"

namespace redgem::ui {
namespace {

using namespace editor_config;

// What each of props::kBodyTypes is for, in their order.
constexpr const char* kBodyDescriptions[] = {"Never moves (floors, walls)", "Falls and gets pushed (crates, balls)",
                                             "Moved by scripts, pushes others", "Walks and jumps (CharacterController)",
                                             "Drives on its wheel children"};
constexpr int kDefaultBody = 0;                                          // the first of props::kBodyTypes
constexpr int kDefaultCollider = 0;                                      // the first of props::kColliders
constexpr const char* kLightTypes[] = {"Directional", "Point", "Spot"};  // in LightType's order
constexpr const char* kPropertyTypes[] = {"Number", "Text", "Toggle"};
constexpr int kNumberType = 0;  // positions in kPropertyTypes
constexpr int kTextType = 1;
constexpr double kDefaultNumber = 0.0;  // what a new Number property starts as
constexpr float kUnitMin = 0.0f;        // metallic and roughness go from 0 to 1
constexpr float kUnitMax = 1.0f;
constexpr float kRadiansToDegrees = 57.29577951f;
constexpr float kAddComponentWidth = 0.7f;  // of the panel width
constexpr float kNoLowerLimit = 0.0f;       // intensities, ranges, clip distances and glow are never negative
constexpr float kPopupFieldWidth = 160.0f;  // pixels: text fields inside the Add Component menus
constexpr const char* kSculptModes[] = {"Raise", "Lower", "Smooth", "Flatten"};  // in SculptMode's order
constexpr float kMinBrushRadius = 1.0f;                                          // meters
constexpr float kMaxBrushRadius = 30.0f;
constexpr float kMinBrushStrength = 0.5f;  // meters per second (see TerrainTool::strength)
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

// Where the parts of a vector and the channels of a color are in the arrays ImGui edits.
enum Axis { kX, kY, kZ };
enum Channel { kRed, kGreen, kBlue, kAlpha };

// Properties edited by dedicated widgets instead of the generic list.
const std::set<std::string>& reserved_props() {
  static const std::set<std::string> keys = {
      props::kScript, props::kBody,    props::kCollider, props::kTrigger, props::kMass,        props::kTag,
      props::kWheel,  props::kVisible, props::kPrefab,   props::kTerrain, props::kTerrainSize, props::kTerrainCells};
  return keys;
}

// Call before a held widget (a drag, a slider, a color picker) changes the selected object: the
// whole interaction is one undo step (see EditStream).
void streamed_edit(PanelContext& context) {
  context.state.inspector.edits.before_change(context.session.document(), context.session.selection());
}

bool is_object(EditorSession& session, int index) { return index >= 0 && index < session.document().node_count(); }

// A text edit to apply: to `object`, the one the field showed when typing started (the selection
// may have changed since).
struct TextEdit {
  int object = kNoIndex;
  std::string text;
};

// Commits a text field when the user leaves it. Each field keeps its own buffer, so moving from one
// field to another never loses typing; clicking another object commits to the one being edited.
std::optional<TextEdit> text_field(InspectorState& state, const char* id, int object, const std::string& current,
                                   const char* hint = "") {
  TextFieldState& field = state.text_fields[ImGui::GetID(id)];
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

// A text property that is not stored while it is empty.
std::optional<PropertyValue> text_or_nothing(const std::string& text) {
  return text.empty() ? std::nullopt : std::optional<PropertyValue>(text);
}

bool has_physics(const PropertyMap& props) {
  return body_config_from_props(props).wants_physics() || props.count(props::kCollider) != 0;
}

// --- Widgets -----------------------------------------------------------------------------------
// Each returns true when it changed the value it shows.

// A labelled row of three numbers.
bool drag_vec3(const char* label, const char* id, Vec3& value, float speed, float lowest = 0.0f, float highest = 0.0f) {
  float numbers[] = {value.x, value.y, value.z};
  property_label(label);
  if (!ImGui::DragFloat3(id, numbers, speed, lowest, highest)) return false;
  value = {numbers[kX], numbers[kY], numbers[kZ]};
  return true;
}

bool edit_color(const char* id, Color& color, ImGuiColorEditFlags flags) {
  float channels[] = {color.r, color.g, color.b, color.a};
  if (!ImGui::ColorEdit4(id, channels, flags)) return false;
  color = {channels[kRed], channels[kGreen], channels[kBlue], channels[kAlpha]};
  return true;
}

// A color without transparency, as a vector or as a Color whose alpha stays as it is.
bool edit_rgb(const char* id, Vec3& color, ImGuiColorEditFlags flags) {
  float channels[] = {color.x, color.y, color.z};
  if (!ImGui::ColorEdit3(id, channels, flags)) return false;
  color = {channels[kRed], channels[kGreen], channels[kBlue]};
  return true;
}

bool edit_rgb(const char* id, Color& color, ImGuiColorEditFlags flags) {
  Vec3 rgb{color.r, color.g, color.b};
  if (!edit_rgb(id, rgb, flags)) return false;
  color = {rgb.x, rgb.y, rgb.z, color.a};
  return true;
}

// A drop-down over the values of an enum, listed by `names` in the enum's order.
template <typename Enum, std::size_t Count>
bool choose(const char* id, Enum& value, const char* const (&names)[Count]) {
  int chosen = static_cast<int>(value);
  if (!ImGui::Combo(id, &chosen, names, static_cast<int>(Count))) return false;
  value = static_cast<Enum>(chosen);
  return true;
}

// The scripts attached to an object, in order.
std::vector<std::string> scripts_of(EditorSession& session, int index) {
  return split_script_list(get_string(session.document().node(index).props, props::kScript).value_or(""));
}

const char* object_icon(const NodeDesc& node) {
  if (node.camera) return ICON_FA_VIDEO;
  if (node.light) return node.light->type == LightType::Directional ? ICON_FA_SUN : ICON_FA_LIGHTBULB;
  if (node.mesh != kNoIndex) return ICON_FA_CUBE;
  return ICON_FA_CIRCLE_DOT;
}

// --- Header ------------------------------------------------------------------------------------

// The object's icon and its name. A name already taken gets a number; an empty one is refused.
void draw_name_field(PanelContext& context, int index) {
  const NodeDesc& node = context.session.document().node(index);
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(accent_color(), "%s", object_icon(node));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kFillWidth);
  const auto name = text_field(context.state.inspector, kNameId, index, node.name);
  if (!name || !is_object(context.session, name->object)) return;
  if (!name->text.empty())
    context.session.document().set_name(name->object, context.session.document().unique_name(name->text));
  context.session.refresh_preview();
}

void draw_tag_field(PanelContext& context, int index) {
  const std::string current = get_string(context.session.document().node(index).props, props::kTag).value_or("");
  property_label("Tag");
  const auto tag = text_field(context.state.inspector, kTagId, index, current, "untagged");
  if (!tag || !is_object(context.session, tag->object)) return;
  set_or_remove(context.session, tag->object, props::kTag, text_or_nothing(tag->text));
}

// Visible and Template, side by side.
void draw_flags(EditorSession& session, int index) {
  const PropertyMap& props = session.document().node(index).props;
  bool visible = get_bool(props, props::kVisible).value_or(true);
  if (ImGui::Checkbox("Visible", &visible)) {
    set_or_remove(session, index, props::kVisible, flag_or_nothing(!visible, false));
  }
  ImGui::SameLine();
  bool prefab = get_bool(props, props::kPrefab).value_or(false);
  if (ImGui::Checkbox("Template", &prefab)) {
    set_or_remove(session, index, props::kPrefab, flag_or_nothing(prefab, true));
  }
  tooltip("Templates are hidden in the game and copied with Scene.spawn(\"Name\", position: ...).");
}

void draw_header(PanelContext& context, int index) {
  draw_name_field(context, index);
  draw_tag_field(context, index);
  draw_flags(context.session, index);
}

// --- Transform ---------------------------------------------------------------------------------

bool drag_rotation(Quat& rotation) {
  Vec3 euler = quat_to_euler_degrees(rotation);
  if (!drag_vec3("Rotation", kRotationId, euler, kDragSpeedRotation)) return false;
  rotation = quat_from_euler_degrees(euler);
  return true;
}

// Only touches the scale when it was itself edited: a mirrored object (negative scale) must survive a move.
bool drag_scale(Vec3& scale) {
  Vec3 edited = scale;
  if (!drag_vec3("Scale", kScaleId, edited, kDragSpeedScale, kMinScale, FLT_MAX)) return false;
  scale = editor_logic::at_least(edited, kMinScale);
  return true;
}

void draw_transform(PanelContext& context, int index) {
  if (!component_header(ICON_FA_UP_DOWN_LEFT_RIGHT, "Transform")) return;
  SceneDocument& doc = context.session.document();
  Transform local = doc.node(index).local;
  const bool moved = drag_vec3("Position", kPositionId, local.position, kDragSpeedPosition);
  const bool rotated = drag_rotation(local.rotation);
  const bool scaled = drag_scale(local.scale);
  if (!moved && !rotated && !scaled) return;
  streamed_edit(context);
  doc.set_local(index, local, false);
  context.session.refresh_preview();
}

// --- Mesh and materials ------------------------------------------------------------------------

// The material's name (or its number) and how many objects show it.
void draw_material_title(const SceneDocument& doc, int material) {
  const std::string& name = doc.material(material).name;
  const std::string title = name.empty() ? "Material " + std::to_string(material) : name;
  ImGui::SeparatorText(title.c_str());
  const int users = doc.material_users(material);
  if (users > 1) ImGui::TextColored(muted_color(), ICON_FA_LINK "  Shared by %d objects", users);
}

bool draw_material_fields(MaterialDesc& edited) {
  bool changed = false;
  property_label(edited.base_color_image != kNoIndex ? "Tint" : "Color");
  changed |= edit_color(kBaseColorId, edited.base_color, ImGuiColorEditFlags_Float);
  property_label("Metallic");
  changed |= ImGui::SliderFloat(kMetallicId, &edited.metallic, kUnitMin, kUnitMax);
  property_label("Roughness");
  changed |= ImGui::SliderFloat(kRoughnessId, &edited.roughness, kUnitMin, kUnitMax);
  property_label("Emission");
  changed |= edit_rgb(kEmissionId, edited.emissive, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
  tooltip("Glow color; values above 1 glow brighter.");
  return changed;
}

void draw_texture_note(const MaterialDesc& material) {
  if (material.base_color_image == kNoIndex && material.normal_image == kNoIndex) return;
  const char* normal_map = material.normal_image != kNoIndex ? " + normal map" : "";
  ImGui::TextColored(muted_color(), ICON_FA_IMAGE "  Textured%s", normal_map);
}

void draw_material(PanelContext& context, int material) {
  SceneDocument& doc = context.session.document();
  MaterialDesc edited = doc.material(material);
  ImGui::PushID(material);
  draw_material_title(doc, material);
  const bool changed = draw_material_fields(edited);
  draw_texture_note(edited);
  if (changed) {
    streamed_edit(context);
    edited.emissive = editor_logic::at_least(edited.emissive, kNoLowerLimit);
    doc.set_material(material, edited, false);
  }
  ImGui::PopID();
}

void draw_mesh(PanelContext& context, int index) {
  const NodeDesc& node = context.session.document().node(index);
  if (node.mesh == kNoIndex) return;
  if (!component_header(ICON_FA_CUBE, "Mesh")) return;
  const MeshDesc& mesh = context.session.scene_desc().meshes[node.mesh];
  property_label("Mesh");
  ImGui::TextUnformatted(mesh.name.empty() ? "(unnamed)" : mesh.name.c_str());
  for (const int material : editor_logic::materials_of(mesh)) draw_material(context, material);
}

// --- Light and camera --------------------------------------------------------------------------

bool draw_light_range(LightDesc& light) {
  property_label("Range");
  const bool changed =
      ImGui::DragFloat(kRangeId, &light.range, kDragSpeedNumber, kNoLowerLimit, FLT_MAX, kMetersFormat);
  tooltip("0 = not set (the engine then uses its default reach)");
  return changed;
}

bool draw_light_fields(LightDesc& light) {
  const bool is_sun = light.type == LightType::Directional;  // as it was before this frame's edits
  bool changed = false;
  property_label("Type");
  changed |= choose(kTypeId, light.type, kLightTypes);
  property_label("Color");
  changed |= edit_rgb(kColorId, light.color, ImGuiColorEditFlags_Float);
  property_label("Intensity");
  changed |= ImGui::DragFloat(kIntensityId, &light.intensity, kDragSpeedIntensity, kNoLowerLimit, FLT_MAX);
  tooltip(is_sun ? "Sun strength" : "Watts (like Blender)");
  if (!is_sun) changed |= draw_light_range(light);
  return changed;
}

void draw_light(PanelContext& context, int index) {
  const NodeDesc& node = context.session.document().node(index);
  if (!node.light) return;
  LightDesc light = *node.light;
  if (!component_header(ICON_FA_LIGHTBULB, "Light")) return;
  if (!draw_light_fields(light)) return;
  streamed_edit(context);
  light.intensity = std::max(light.intensity, kNoLowerLimit);
  light.range = std::max(light.range, kNoLowerLimit);
  context.session.document().set_light(index, light, false);
  context.session.refresh_preview();
}

bool draw_camera_fields(CameraDesc& camera, float& fov_degrees) {
  bool changed = false;
  property_label("Field of view");
  changed |= ImGui::SliderFloat(kFovId, &fov_degrees, kMinFovDegrees, kMaxFovDegrees, "%.0f deg");
  property_label("Near");
  changed |= ImGui::DragFloat(kNearId, &camera.near_plane, kDragSpeedScale, kMinClipDistance, FLT_MAX);
  property_label("Far");
  changed |= ImGui::DragFloat(kFarId, &camera.far_plane, kDragSpeedNumber, kNoLowerLimit, FLT_MAX);
  tooltip("0 = infinite");
  return changed;
}

void draw_main_camera_note(const EditorSession& session, int index) {
  if (session.scene().main_camera() != index) return;
  ImGui::TextColored(muted_color(), ICON_FA_STAR "  The game starts with this camera");
}

void draw_camera(PanelContext& context, int index) {
  const NodeDesc& node = context.session.document().node(index);
  if (!node.camera) return;
  CameraDesc camera = *node.camera;
  if (!component_header(ICON_FA_VIDEO, "Camera")) return;
  float fov = camera.fov_y_radians * kRadiansToDegrees;
  const bool changed = draw_camera_fields(camera, fov);
  draw_main_camera_note(context.session, index);
  if (!changed) return;
  streamed_edit(context);
  camera.fov_y_radians = std::clamp(fov, kMinFovDegrees, kMaxFovDegrees) / kRadiansToDegrees;
  camera.near_plane = std::max(camera.near_plane, kMinClipDistance);
  context.session.document().set_camera(index, camera, false);
  context.session.refresh_preview();
}

// --- Scripts -----------------------------------------------------------------------------------

void write_scripts(EditorSession& session, int index, const std::vector<std::string>& scripts) {
  set_or_remove(session, index, props::kScript, text_or_nothing(join_script_list(scripts)));
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

// One attached script as a card of its own. True when its close button was pressed.
bool draw_script(PanelContext& context, const std::string& script) {
  bool removed = false;
  const std::string title = class_name_for_script(script);
  const bool open = component_header(ICON_FA_GEM, title.c_str(), &removed);
  if (open && !removed) draw_script_card(context, script);
  return removed;
}

void draw_scripts(PanelContext& context, int index) {
  const std::vector<std::string> scripts = scripts_of(context.session, index);
  for (std::size_t i = 0; i < scripts.size(); ++i) {
    ImGui::PushID(static_cast<int>(i));
    const bool removed = draw_script(context, scripts[i]);
    ImGui::PopID();
    if (!removed) continue;
    write_scripts(context.session, index, editor_logic::without_item(scripts, i));
    return;  // the cards after it are drawn from the new list on the next frame
  }
}

// --- Physics -----------------------------------------------------------------------------------

// One undo step for the whole component.
void remove_physics(EditorSession& session, int index) {
  SceneDocument& doc = session.document();
  doc.checkpoint();
  for (const char* key : {props::kBody, props::kCollider, props::kMass, props::kTrigger}) {
    if (doc.node(index).props.count(key) != 0) doc.remove_prop(index, key, false);
  }
  doc.drop_checkpoint_if_unchanged();
  session.refresh_preview();
}

// The brush that sculpts a terrain in the Scene view.
void draw_brush_mode(TerrainTool& tool) {
  property_label("Brush");
  ImGui::SetNextItemWidth(kFillWidth);
  choose(kBrushModeId, tool.mode, kSculptModes);
}

void draw_brush_size(TerrainTool& tool) {
  property_label("Radius");
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::SliderFloat(kBrushRadiusId, &tool.radius, kMinBrushRadius, kMaxBrushRadius, kMetersFormat);
  property_label("Strength");
  ImGui::SetNextItemWidth(kFillWidth);
  ImGui::SliderFloat(kBrushStrengthId, &tool.strength, kMinBrushStrength, kMaxBrushStrength, "%.1f");
}

// The brush that sculpts a terrain in the Scene view.
void draw_terrain(PanelContext& context, int index) {
  if (context.terrain == nullptr || !context.session.is_terrain(index)) return;
  if (!component_header(ICON_FA_MOUNTAIN, "Terrain")) return;
  TerrainTool& tool = *context.terrain;
  property_label("Sculpt");
  ImGui::Checkbox(kSculptId, &tool.sculpting);
  tooltip("While on, drag the left mouse button over the terrain in the Scene view. Shift reverses Raise and Lower.");
  draw_brush_mode(tool);
  draw_brush_size(tool);
}

// Each of the rows below returns true when it changed the object: the card is then drawn again
// from the new properties on the next frame.
bool draw_trigger_row(EditorSession& session, int index, bool& trigger) {
  property_label("Trigger");
  if (ImGui::Checkbox(kTriggerId, &trigger)) {
    set_or_remove(session, index, props::kTrigger, flag_or_nothing(trigger, true));
    return true;
  }
  tooltip("Detects overlaps (on_trigger_enter) instead of colliding.");
  return false;
}

bool draw_body_row(EditorSession& session, int index, const PropertyMap& props) {
  const int count = static_cast<int>(std::size(props::kBodyTypes));
  const std::string current = get_string(props, props::kBody).value_or(props::kBodyTypes[kDefaultBody]);
  int type = editor_logic::index_of(props::kBodyTypes, count, current, kDefaultBody);
  property_label("Body");
  if (ImGui::Combo(kBodyId, &type, props::kBodyTypes, count)) {
    set_or_remove(session, index, props::kBody, PropertyValue(std::string(props::kBodyTypes[type])));
    return true;
  }
  tooltip(kBodyDescriptions[type]);
  return false;
}

bool draw_collider_row(EditorSession& session, int index, const PropertyMap& props) {
  const int count = static_cast<int>(std::size(props::kColliders));
  const std::string current = get_string(props, props::kCollider).value_or(props::kColliders[kDefaultCollider]);
  int collider = editor_logic::index_of(props::kColliders, count, current, kDefaultCollider);
  property_label("Collider");
  if (!ImGui::Combo(kColliderId, &collider, props::kColliders, count)) return false;
  set_or_remove(session, index, props::kCollider, PropertyValue(std::string(props::kColliders[collider])));
  return true;
}

void draw_mass_row(PanelContext& context, int index, float mass) {
  property_label("Mass");
  if (!ImGui::DragFloat(kMassId, &mass, kDragSpeedNumber, kMinMass, FLT_MAX, "%.2f kg")) return;
  streamed_edit(context);
  context.session.document().set_prop(index, props::kMass, static_cast<double>(std::max(mass, kMinMass)), false);
}

// The rows of the Physics card, until one of them changes the object.
void draw_physics_rows(PanelContext& context, int index, const BodyConfig& body) {
  const PropertyMap& props = context.session.document().node(index).props;
  bool trigger = get_bool(props, props::kTrigger).value_or(false);
  if (draw_trigger_row(context.session, index, trigger)) return;
  if (!trigger && draw_body_row(context.session, index, props)) return;
  if (draw_collider_row(context.session, index, props)) return;
  if (body.type == BodyType::Dynamic || body.type == BodyType::Vehicle) draw_mass_row(context, index, body.mass);
  for (const std::string& warning : body.warnings) {
    ImGui::TextColored(warning_color(), ICON_FA_TRIANGLE_EXCLAMATION "  %s", warning.c_str());
  }
}

void draw_physics(PanelContext& context, int index) {
  const PropertyMap& props = context.session.document().node(index).props;
  if (!has_physics(props)) return;
  const BodyConfig body = body_config_from_props(props);
  bool removed = false;
  const bool open = component_header(ICON_FA_WEIGHT_HANGING, "Physics", &removed);
  if (removed) {
    remove_physics(context.session, index);
  } else if (open) {
    draw_physics_rows(context, index, body);
  }
}

// Wheels are children of a vehicle: shown on the child itself.
void draw_wheel(EditorSession& session, int index) {
  const NodeDesc& node = session.document().node(index);
  if (node.parent == kNoIndex) return;
  const auto parent_body = get_string(session.document().node(node.parent).props, props::kBody);
  const bool is_wheel = get_bool(node.props, props::kWheel).value_or(false);
  if (parent_body != std::optional<std::string>(props::kBodyVehicle) && !is_wheel) return;
  if (!component_header(ICON_FA_CAR, "Vehicle wheel")) return;
  bool wheel = is_wheel;
  property_label("Is a wheel");
  if (ImGui::Checkbox(kWheelId, &wheel)) set_or_remove(session, index, props::kWheel, flag_or_nothing(wheel, true));
}

// --- Custom properties -------------------------------------------------------------------------

bool is_custom(const std::string& key) { return reserved_props().count(key) == 0; }

// The widget of one custom property, by what it holds: a checkbox, a number or a text field.
void draw_flag_value(EditorSession& session, int index, const std::string& key, bool flag) {
  if (ImGui::Checkbox(kValueId, &flag)) set_or_remove(session, index, key.c_str(), PropertyValue(flag));
}

void draw_number_value(PanelContext& context, int index, const std::string& key, double number) {
  if (!ImGui::DragScalar(kValueId, ImGuiDataType_Double, &number, kDragSpeedNumber)) return;
  streamed_edit(context);
  context.session.document().set_prop(index, key, number, false);
}

void draw_text_value(PanelContext& context, int index, const std::string& key, const std::string& current) {
  const auto text = text_field(context.state.inspector, kValueId, index, current);
  if (!text || !is_object(context.session, text->object)) return;
  set_or_remove(context.session, text->object, key.c_str(), PropertyValue(text->text));
}

// The widget of one custom property, by what it holds: a checkbox, a number or a text field.
void draw_custom_value(PanelContext& context, int index, const std::string& key, const PropertyValue& value) {
  if (const auto* flag = std::get_if<bool>(&value)) {
    draw_flag_value(context.session, index, key, *flag);
  } else if (const auto* number = std::get_if<double>(&value)) {
    draw_number_value(context, index, key, *number);
  } else {
    draw_text_value(context, index, key, std::get<std::string>(value));
  }
}

// One custom property: a button that removes it, its name and its value.
void draw_custom_row(PanelContext& context, int index, const std::string& key, const PropertyValue& value) {
  if (ImGui::SmallButton(ICON_FA_XMARK)) {
    set_or_remove(context.session, index, key.c_str(), std::nullopt);
    return;
  }
  tooltip("Remove");
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(muted_color(), "%s", key.c_str());
  ImGui::SameLine(kLabelColumnWidth);
  ImGui::SetNextItemWidth(kFillWidth);
  draw_custom_value(context, index, key, value);
}

void draw_custom_props(PanelContext& context, int index) {
  const PropertyMap props = context.session.document().node(index).props;  // copy: edits change the map
  const bool any = std::any_of(props.begin(), props.end(), [](const auto& entry) { return is_custom(entry.first); });
  if (!any) return;
  if (!component_header(ICON_FA_SLIDERS, "Properties")) return;
  for (const auto& [key, value] : props) {
    if (!is_custom(key)) continue;
    ImGui::PushID(key.c_str());
    draw_custom_row(context, index, key, value);
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

// The game's scripts the object does not have yet.
void draw_attachable_scripts(PanelContext& context, int index, const std::vector<std::string>& attached) {
  for (const std::string& script : context.files.scripts) {
    if (std::find(attached.begin(), attached.end(), script) != attached.end()) continue;
    if (ImGui::MenuItem((std::string(ICON_FA_GEM "  ") + script).c_str())) context.session.attach_script(index, script);
  }
}

void add_script_menu(PanelContext& context, int index) {
  const std::vector<std::string> attached = scripts_of(context.session, index);
  std::string& new_name = context.state.inspector.new_script_name;
  ImGui::SetNextItemWidth(kPopupFieldWidth);
  if (draw_new_script_form(kNewScriptId, "new_script_name", ICON_FA_PLUS " New", new_name)) {
    create_and_attach_script(context, index, new_name);
  }
  if (!context.files.scripts.empty()) ImGui::Separator();
  draw_attachable_scripts(context, index, attached);
}

// What a new property of each kind starts as.
PropertyValue initial_value(int type) {
  if (type == kNumberType) return PropertyValue(kDefaultNumber);
  return type == kTextType ? PropertyValue(std::string()) : PropertyValue(false);
}

// A name the object can take as a new property: not empty, not taken, not one with a widget of its own.
bool can_add_property(const PropertyMap& props, const std::string& key) {
  return !key.empty() && reserved_props().count(key) == 0 && props.count(key) == 0;
}

void add_property_menu(PanelContext& context, int index) {
  InspectorState& state = context.state.inspector;
  std::string& key = state.new_property_key;
  ImGui::SetNextItemWidth(kPopupFieldWidth);
  ImGui::InputTextWithHint(kNewPropertyId, "name", &key);
  ImGui::SetNextItemWidth(kPopupFieldWidth);
  ImGui::Combo(kTypeId, &state.new_property_type, kPropertyTypes, static_cast<int>(std::size(kPropertyTypes)));
  ImGui::BeginDisabled(!can_add_property(context.session.document().node(index).props, key));
  if (ImGui::Button(ICON_FA_PLUS " Add property")) {
    set_or_remove(context.session, index, key.c_str(), initial_value(state.new_property_type));
    key.clear();
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
}

void add_body_menu(EditorSession& session, int index) {
  for (int i = 0; i < static_cast<int>(std::size(props::kBodyTypes)); ++i) {
    if (ImGui::MenuItem(props::kBodyTypes[i])) {
      set_or_remove(session, index, props::kBody, PropertyValue(std::string(props::kBodyTypes[i])));
    }
    tooltip(kBodyDescriptions[i]);
  }
}

// What the Add Component button offers.
void draw_component_choices(PanelContext& context, int index) {
  EditorSession& session = context.session;
  menu(ICON_FA_GEM "  Script", [&context, index] { add_script_menu(context, index); });
  const bool no_physics = !has_physics(session.document().node(index).props);
  menu(ICON_FA_WEIGHT_HANGING "  Physics body", [&session, index] { add_body_menu(session, index); }, no_physics);
  if (ImGui::MenuItem(ICON_FA_BULLSEYE "  Trigger", nullptr, false, no_physics)) {
    set_or_remove(session, index, props::kTrigger, PropertyValue(true));
  }
  menu(ICON_FA_SLIDERS "  Property", [&context, index] { add_property_menu(context, index); });
}

void draw_add_component(PanelContext& context, int index) {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  const float available = ImGui::GetContentRegionAvail().x;
  const float width = available * kAddComponentWidth;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + editor_logic::centered_start(available, width));
  if (ImGui::Button(ICON_FA_PLUS "  Add Component", ImVec2(width, 0.0f))) ImGui::OpenPopup(kAddComponentPopup);
  popup(kAddComponentPopup, [&context, index] { draw_component_choices(context, index); });
}

// Where the running game has the object now.
void draw_live_transform(const Transform& world) {
  if (!component_header(ICON_FA_UP_DOWN_LEFT_RIGHT, "Transform (live)")) return;
  const Vec3 euler = quat_to_euler_degrees(world.rotation);
  property_label("Position");
  ImGui::Text("%.2f  %.2f  %.2f", world.position.x, world.position.y, world.position.z);
  property_label("Rotation");
  ImGui::Text("%.1f  %.1f  %.1f", euler.x, euler.y, euler.z);
}

void draw_runtime_view(EditorSession& session, int index) {
  const Scene& scene = session.scene();
  if (!scene.is_alive(index)) {
    ImGui::TextColored(muted_color(), "Destroyed during play.");
    return;
  }
  draw_live_transform(scene.world_transform(index));
  ImGui::Spacing();
  ImGui::TextColored(muted_color(), ICON_FA_CIRCLE_INFO "  Stop the game to edit.");
}

// The selection as cards, one per component it has.
void draw_components(PanelContext& context, int index) {
  EditorSession& session = context.session;
  draw_header(context, index);
  draw_transform(context, index);
  draw_mesh(context, index);
  draw_light(context, index);
  draw_camera(context, index);
  draw_terrain(context, index);
  draw_scripts(context, index);
  draw_physics(context, index);
  draw_wheel(session, index);
  draw_custom_props(context, index);
  draw_add_component(context, index);
}

// Widget IDs do not include the object: a text field being edited when another object gets
// selected still commits to the object it was showing (see text_field).
void draw_selection(PanelContext& context, int index) {
  EditorSession& session = context.session;
  if (session.play_state() == PlayState::Editing) {
    draw_components(context, index);
  } else {
    ImGui::TextUnformatted(session.document().node(index).name.c_str());
    draw_runtime_view(session, index);
  }
}

}  // namespace

void open_script(ScriptWorkspace& scripts, const std::string& file_name) {
  std::string error;
  if (scripts.open(file_name, error) == nullptr) log::error(error);
}

void draw_inspector(PanelContext& context) {
  ImGui::Begin(kInspectorWindow);
  const ObjectId index = context.session.selection();
  if (index == kNoObject) {
    ImGui::TextColored(muted_color(), ICON_FA_ARROW_POINTER "  Select an object in the Hierarchy or the Scene.");
  } else {
    draw_selection(context, index);
  }
  ImGui::End();
}

}  // namespace redgem::ui
