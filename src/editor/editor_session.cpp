#include "editor/editor_session.hpp"

#include <algorithm>
#include <utility>

#include "core/files.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "editor/editor_logic.hpp"
#include "scene/props.hpp"
#include "script/script_naming.hpp"

namespace redgem {
namespace {

std::filesystem::file_time_type write_time(const std::filesystem::path& path) {
  std::error_code error;
  const auto time = std::filesystem::last_write_time(path, error);
  return error ? std::filesystem::file_time_type{} : time;
}

// Defaults of objects created from the editor's menus.
constexpr const char* kNewEmptyName = "GameObject";
constexpr const char* kNewTerrainName = "Terrain";
constexpr const char* kNewCameraName = "Camera";
constexpr const char* kNewEnvironmentName = "Environment";
constexpr float kNewTerrainSize = 100.0f;                     // meters across
constexpr int kNewTerrainCells = 64;                          // quads per side: about 1.5 m each
constexpr Color kNewTerrainColor{0.33f, 0.46f, 0.25f, 1.0f};  // grass
constexpr float kGroundLevel = 0.0f;                          // meters: new terrains lie at y = 0
constexpr float kTerrainRayReach = 2000.0f;                   // meters a ray is followed looking for the ground
constexpr Color kNewShapeColor{0.8f, 0.8f, 0.8f, 1.0f};       // light gray
constexpr Color kNewLightColor{1.0f, 1.0f, 1.0f, 1.0f};       // white
constexpr float kNewSunIntensity = 3.0f;                      // W/m2, like Blender's sun strength
constexpr float kNewLampIntensity = 500.0f;                   // watts, like Blender's default point light
constexpr float kNewCameraFov = 0.87f;                        // ~50 degrees
// New suns shine down at an angle instead of straight along -Z.
constexpr Vec3 kNewSunDirection{-0.4f, -1.0f, -0.6f};

// What GameObject > Environment starts with: a sunset (colors as "#rrggbb").
constexpr std::pair<const char*, const char*> kSunsetColors[] = {{props::environment::kSkyTop, "#33457f"},
                                                                 {props::environment::kSkyHorizon, "#ffab73"},
                                                                 {props::environment::kGroundColor, "#4a3c3a"},
                                                                 {props::environment::kFogColor, "#e8a988"}};
constexpr std::pair<const char*, double> kSunsetNumbers[] = {
    {props::environment::kFogDensity, 0.003}, {props::environment::kAmbient, 0.8},
    {props::environment::kExposure, 1.0},     {props::environment::kBloom, 0.45},
    {props::environment::kVignette, 0.35},    {props::environment::kSunGlow, 1.6},
    {props::environment::kWind, 0.6}};

const char* light_name(LightType type) {
  return type == LightType::Directional ? "Sun" : type == LightType::Spot ? "Spot Light" : "Point Light";
}

// A game running inside the editor: the edited scene instead of the file, with the editor's devices.
EngineOptions runtime_options(const std::filesystem::path& game_dir, const SceneDesc& scene,
                              const PlayDevices& devices) {
  EngineOptions options;
  options.game_dir = game_dir;
  options.scene = &scene;
  options.input = &devices.input;
  options.audio = devices.audio;
  options.presenter = devices.presenter;
  return options;
}

}  // namespace

std::variant<std::unique_ptr<EditorSession>, std::string> EditorSession::open(const std::filesystem::path& folder) {
  const std::filesystem::path game_dir = normalized_folder(folder);
  const std::filesystem::path scene_path = game_dir / config::kSceneFileName;
  auto opened = SceneDocument::open(scene_path);
  if (auto* error = std::get_if<std::string>(&opened)) return *error;
  std::unique_ptr<EditorSession> session(new EditorSession(game_dir, std::move(std::get<SceneDocument>(opened))));
  session->loaded_write_time_ = write_time(scene_path);
  session->refresh_preview();
  return session;
}

EditorSession::EditorSession(std::filesystem::path game_dir, SceneDocument document)
    : game_dir_(std::move(game_dir)), document_(std::move(document)) {}

const Scene& EditorSession::scene() const { return runtime_ ? runtime_->scene() : preview_; }

void EditorSession::refresh_preview() {
  preview_ = Scene::from_desc(document_.desc());
  if (selection_ >= document_.node_count()) selection_ = kNoObject;
}

void EditorSession::select(ObjectId id) { selection_ = id >= 0 && id < document_.node_count() ? id : kNoObject; }

void EditorSession::select_by_name(const std::string& name) { select(document_.desc().find_node(name)); }

void EditorSession::duplicate_selection() {
  if (play_state_ != PlayState::Editing || selection_ == kNoObject) return;
  selection_ = document_.duplicate(selection_);
  refresh_preview();
}

void EditorSession::delete_selection() {
  if (play_state_ != PlayState::Editing || selection_ == kNoObject) return;
  document_.remove(selection_);
  selection_ = kNoObject;
  refresh_preview();
}

ObjectId EditorSession::place_new(int node, Vec3 position) {
  Transform local = document_.node(node).local;
  local.position = position;
  document_.set_local(node, local, false);  // part of the creation's undo step
  refresh_preview();
  selection_ = node;
  return node;
}

ObjectId EditorSession::create_empty(Vec3 position) {
  if (play_state_ != PlayState::Editing) return kNoObject;
  return place_new(document_.add_empty(document_.unique_name(kNewEmptyName)), position);
}

ObjectId EditorSession::create_primitive(PrimitiveShape shape, Vec3 position) {
  if (play_state_ != PlayState::Editing) return kNoObject;
  const std::string name = document_.unique_name(primitive_name(shape));
  return place_new(document_.add_primitive(shape, name, kNewShapeColor), position);
}

ObjectId EditorSession::create_terrain(Vec3 position) {
  if (play_state_ != PlayState::Editing) return kNoObject;
  const int node = document_.add_terrain(document_.unique_name(kNewTerrainName), {kNewTerrainSize, kNewTerrainCells},
                                         kNewTerrainColor);
  return place_new(node, {position.x, kGroundLevel, position.z});  // wherever the camera looks from
}

bool EditorSession::is_terrain(ObjectId id) const { return document_.terrain_grid(id).has_value(); }

std::string EditorSession::selected_name() const {
  return selection_ != kNoObject ? document_.node(selection_).name : "";
}

std::optional<Vec3> EditorSession::terrain_hit(ObjectId terrain, Vec3 origin, Vec3 direction) const {
  const auto grid = document_.terrain_grid(terrain);
  if (!grid) return std::nullopt;
  const Transform world = preview_.world_transform(terrain);
  const Vec3 local_origin = editor_logic::to_local(world, origin);
  const Vec3 local_direction = normalize(editor_logic::to_local(world, origin + direction) - local_origin);
  const MeshDesc& mesh = document_.desc().meshes[static_cast<std::size_t>(document_.node(terrain).mesh)];
  const PrimitiveDesc& ground = mesh.primitives.front();
  const auto hit = terrain_raycast(ground, *grid, local_origin, local_direction, kTerrainRayReach);
  if (!hit) return std::nullopt;
  return editor_logic::to_world(world, *hit);
}

bool EditorSession::sculpt_terrain(ObjectId terrain, Vec3 world_point, float radius, float amount, SculptMode mode) {
  if (play_state_ != PlayState::Editing || !document_.terrain_grid(terrain)) return false;
  // A scaled terrain: the brush is in meters of the world, the grid in its own.
  const TerrainBrush brush =
      editor_logic::local_brush(preview_.world_transform(terrain), world_point, radius, amount, mode);
  if (!document_.sculpt_terrain(terrain, brush)) return false;
  refresh_preview();
  return true;
}

ObjectId EditorSession::create_light(LightType type, Vec3 position) {
  if (play_state_ != PlayState::Editing) return kNoObject;
  const float intensity = type == LightType::Directional ? kNewSunIntensity : kNewLampIntensity;
  const int node = document_.add_light(type, document_.unique_name(light_name(type)), kNewLightColor, intensity);
  if (type != LightType::Point) {
    Transform local = document_.node(node).local;
    local.rotation = quat_look_rotation(normalize(kNewSunDirection), Vec3::up());
    document_.set_local(node, local, false);
  }
  return place_new(node, position);
}

ObjectId EditorSession::create_camera(Vec3 position) {
  if (play_state_ != PlayState::Editing) return kNoObject;
  return place_new(document_.add_camera(document_.unique_name(kNewCameraName), kNewCameraFov), position);
}

ObjectId EditorSession::find_environment() const {
  for (int i = 0; i < document_.node_count(); ++i) {
    if (get_bool(document_.node(i).props, props::kEnvironment).value_or(false)) return i;
  }
  return kNoObject;
}

ObjectId EditorSession::create_environment() {
  if (play_state_ != PlayState::Editing) return kNoObject;
  const ObjectId existing = find_environment();
  if (existing != kNoObject) {
    selection_ = existing;
    return existing;
  }
  const int node = document_.add_empty(document_.unique_name(kNewEnvironmentName));
  document_.set_prop(node, props::kEnvironment, true, false);  // same undo step as the creation
  for (const auto& [key, value] : kSunsetColors) document_.set_prop(node, key, std::string(value), false);
  for (const auto& [key, value] : kSunsetNumbers) document_.set_prop(node, key, value, false);
  return place_new(node, Vec3::zero());
}

ObjectId EditorSession::import_model(const std::filesystem::path& path, Vec3 position, std::string& error,
                                     ObjectId parent) {
  if (play_state_ != PlayState::Editing) {
    error = "stop the game to edit the scene";
    return kNoObject;
  }
  const int node = document_.import_model(path, error, parent);
  if (node == kNoIndex) return kNoObject;
  log::info("imported " + path.filename().string());
  if (parent == kNoObject) return place_new(node, position);
  refresh_preview();  // the parent's world placement is needed below
  Transform world = preview_.world_transform(node);
  world.position = position;
  return place_new(node, relative_to(preview_.world_transform(parent), world).position);
}

void EditorSession::reparent(ObjectId child, ObjectId parent) {
  if (play_state_ != PlayState::Editing || child == kNoObject) return;
  const bool child_selected = selection_ == child;
  const std::string selected = selected_name();
  const int moved = document_.reparent(child, parent);
  refresh_preview();
  if (child_selected) {
    selection_ = moved;
  } else {
    select_by_name(selected);  // indices of other objects may shift
  }
}

void EditorSession::attach_script(ObjectId id, const std::string& file) {
  if (play_state_ != PlayState::Editing || id == kNoObject) return;
  std::vector<std::string> scripts =
      split_script_list(get_string(document_.node(id).props, props::kScript).value_or(""));
  if (std::find(scripts.begin(), scripts.end(), file) != scripts.end()) return;
  scripts.push_back(file);
  document_.set_prop(id, props::kScript, join_script_list(scripts));
  refresh_preview();
}

void EditorSession::refresh_and_reselect(const std::string& name) {
  refresh_preview();
  select_by_name(name);  // indices may have shifted; names are unique
}

void EditorSession::undo() {
  if (play_state_ != PlayState::Editing) return;
  const std::string selected = selected_name();
  if (document_.undo()) refresh_and_reselect(selected);
}

void EditorSession::redo() {
  if (play_state_ != PlayState::Editing) return;
  const std::string selected = selected_name();
  if (document_.redo()) refresh_and_reselect(selected);
}

bool EditorSession::save(std::string& error) {
  if (!document_.save(error)) return false;
  loaded_write_time_ = write_time(document_.path());
  external_change_pending_ = false;
  log::info("saved " + document_.path().string());
  return true;
}

bool EditorSession::play(const PlayDevices& devices, std::string& error) {
  if (play_state_ != PlayState::Editing) return true;
  auto created = Engine::create(runtime_options(game_dir_, document_.desc(), devices));
  if (auto* message = std::get_if<std::string>(&created)) {
    error = *message;
    return false;
  }
  runtime_ = std::move(std::get<std::unique_ptr<Engine>>(created));
  game_audio_ = devices.audio;
  play_state_ = PlayState::Playing;
  log::info("play");
  return true;
}

void EditorSession::pause(bool paused) {
  if (play_state_ == PlayState::Editing) return;
  play_state_ = paused ? PlayState::Paused : PlayState::Playing;
}

void EditorSession::stop() {
  if (play_state_ == PlayState::Editing) return;
  runtime_.reset();
  // The game's music ends with the game, whoever ended it: Stop, or the game itself (Game.quit).
  if (game_audio_ != nullptr) game_audio_->stop_music();
  game_audio_ = nullptr;
  play_state_ = PlayState::Editing;
  refresh_preview();
  log::info("stop");
}

void EditorSession::tick(float frame_seconds) {
  if (play_state_ != PlayState::Playing || !runtime_) return;
  runtime_->frame(frame_seconds);
  if (runtime_->finished()) stop();  // a script called Game.quit
}

bool EditorSession::reload_if_changed_on_disk() {
  const auto time = write_time(document_.path());
  if (time == loaded_write_time_ || play_state_ != PlayState::Editing) return false;
  // Each version of the file is handled once. A reload that fails because the file is still being
  // written is tried again when the writer finishes (the file's time changes again), without
  // logging the same error on every check in between.
  loaded_write_time_ = time;
  if (document_.dirty()) {
    external_change_pending_ = true;
    log::warning(document_.path().string() + " changed on disk while you have unsaved edits");
    return false;
  }
  return reload_from_disk();
}

bool EditorSession::reload_from_disk() {
  const std::filesystem::path path = document_.path();
  auto opened = SceneDocument::open(path);
  if (auto* error = std::get_if<std::string>(&opened)) {
    log::error("cannot reload " + path.string() + ": " + *error);
    return false;
  }
  const std::string selected = selected_name();
  document_ = std::move(std::get<SceneDocument>(opened));
  loaded_write_time_ = write_time(path);
  external_change_pending_ = false;
  refresh_and_reselect(selected);
  log::info("reloaded " + path.string());
  return true;
}

}  // namespace redgem
