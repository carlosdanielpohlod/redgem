#include "editor/editor_session.hpp"

#include <algorithm>
#include <utility>

#include "app/game_project.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "script/script_naming.hpp"

namespace redgem {
namespace {

std::filesystem::file_time_type write_time(const std::filesystem::path& path) {
  std::error_code error;
  const auto time = std::filesystem::last_write_time(path, error);
  return error ? std::filesystem::file_time_type{} : time;
}

// Defaults of objects created from the editor's menus.
constexpr Color kNewShapeColor{0.8f, 0.8f, 0.8f, 1.0f};
constexpr Color kNewLightColor{1.0f, 1.0f, 1.0f, 1.0f};
constexpr float kNewSunIntensity = 3.0f;
constexpr float kNewLampIntensity = 500.0f;  // watts, like Blender's default point light
constexpr float kNewCameraFov = 0.87f;       // ~50 degrees
// New suns shine down at an angle instead of straight along -Z.
constexpr Vec3 kNewSunDirection{-0.4f, -1.0f, -0.6f};

// What GameObject > Environment starts with: a sunset (colors as "#rrggbb").
constexpr const char* kEnvironmentProp = "environment";
constexpr std::pair<const char*, const char*> kSunsetColors[] = {
    {"sky_top", "#33457f"}, {"sky_horizon", "#ffab73"}, {"ground_color", "#4a3c3a"}, {"fog_color", "#e8a988"}};
constexpr std::pair<const char*, double> kSunsetNumbers[] = {
    {"fog_density", 0.003}, {"ambient", 0.8}, {"exposure", 1.0}, {"bloom", 0.45},
    {"vignette", 0.35},     {"sun_glow", 1.6}, {"wind", 0.6}};

const char* light_name(LightType type) {
  return type == LightType::Directional ? "Sun" : type == LightType::Spot ? "Spot Light" : "Point Light";
}

}  // namespace

std::variant<std::unique_ptr<EditorSession>, std::string> EditorSession::open(const std::filesystem::path& folder) {
  const std::filesystem::path game_dir = normalized_folder(folder);
  const std::filesystem::path scene_path = game_dir / config::kSceneFileName;
  auto opened = SceneDocument::open(scene_path);
  if (auto* error = std::get_if<std::string>(&opened)) return *error;
  std::unique_ptr<EditorSession> session(new EditorSession());
  session->game_dir_ = game_dir;
  session->document_.emplace(std::move(std::get<SceneDocument>(opened)));
  session->loaded_write_time_ = write_time(scene_path);
  session->refresh_preview();
  return session;
}

const Scene& EditorSession::scene() const { return runtime_ ? runtime_->scene() : preview_; }

void EditorSession::refresh_preview() {
  preview_ = Scene::from_desc(document_->desc());
  if (selection_ >= document_->node_count()) selection_ = kNoObject;
}

void EditorSession::select(ObjectId id) {
  selection_ = id >= 0 && id < document_->node_count() ? id : kNoObject;
}

void EditorSession::select_by_name(const std::string& name) { select(document_->desc().find_node(name)); }

void EditorSession::duplicate_selection() {
  if (play_state_ != PlayState::Editing || selection_ == kNoObject) return;
  selection_ = document_->duplicate(selection_);
  refresh_preview();
}

void EditorSession::delete_selection() {
  if (play_state_ != PlayState::Editing || selection_ == kNoObject) return;
  document_->remove(selection_);
  selection_ = kNoObject;
  refresh_preview();
}

ObjectId EditorSession::place_new(int node, Vec3 position) {
  Transform local = document_->node(node).local;
  local.position = position;
  document_->set_local(node, local, false);  // part of the creation's undo step
  refresh_preview();
  selection_ = node;
  return node;
}

ObjectId EditorSession::create_empty(Vec3 position) {
  if (play_state_ != PlayState::Editing) return kNoObject;
  return place_new(document_->add_empty(document_->unique_name("GameObject")), position);
}

ObjectId EditorSession::create_primitive(PrimitiveShape shape, Vec3 position) {
  if (play_state_ != PlayState::Editing) return kNoObject;
  const std::string name = document_->unique_name(primitive_name(shape));
  return place_new(document_->add_primitive(shape, name, kNewShapeColor), position);
}

ObjectId EditorSession::create_light(LightType type, Vec3 position) {
  if (play_state_ != PlayState::Editing) return kNoObject;
  const float intensity = type == LightType::Directional ? kNewSunIntensity : kNewLampIntensity;
  const int node = document_->add_light(type, document_->unique_name(light_name(type)), kNewLightColor, intensity);
  if (type != LightType::Point) {
    Transform local = document_->node(node).local;
    local.rotation = quat_look_rotation(normalize(kNewSunDirection), Vec3::up());
    document_->set_local(node, local, false);
  }
  return place_new(node, position);
}

ObjectId EditorSession::create_camera(Vec3 position) {
  if (play_state_ != PlayState::Editing) return kNoObject;
  return place_new(document_->add_camera(document_->unique_name("Camera"), kNewCameraFov), position);
}

ObjectId EditorSession::create_environment() {
  if (play_state_ != PlayState::Editing) return kNoObject;
  for (int i = 0; i < document_->node_count(); ++i) {
    if (get_bool(document_->node(i).props, kEnvironmentProp).value_or(false)) {
      selection_ = i;
      return i;
    }
  }
  const int node = document_->add_empty(document_->unique_name("Environment"));
  document_->set_prop(node, kEnvironmentProp, true, false);  // same undo step as the creation
  for (const auto& [key, value] : kSunsetColors) document_->set_prop(node, key, std::string(value), false);
  for (const auto& [key, value] : kSunsetNumbers) document_->set_prop(node, key, value, false);
  return place_new(node, Vec3::zero());
}

ObjectId EditorSession::import_model(const std::filesystem::path& path, Vec3 position, std::string& error,
                                     ObjectId parent) {
  if (play_state_ != PlayState::Editing) {
    error = "stop the game to edit the scene";
    return kNoObject;
  }
  const int node = document_->import_model(path, error, parent);
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
  const std::string selected = selection_ != kNoObject ? document_->node(selection_).name : "";
  const int moved = document_->reparent(child, parent);
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
      split_script_list(get_string(document_->node(id).props, config::kPropScript).value_or(""));
  if (std::find(scripts.begin(), scripts.end(), file) != scripts.end()) return;
  scripts.push_back(file);
  document_->set_prop(id, config::kPropScript, join_script_list(scripts));
  refresh_preview();
}

void EditorSession::undo() {
  if (play_state_ != PlayState::Editing) return;
  const std::string selected = selection_ != kNoObject ? document_->node(selection_).name : "";
  if (!document_->undo()) return;
  refresh_preview();
  select_by_name(selected);  // indices may have shifted; names are unique
}

void EditorSession::redo() {
  if (play_state_ != PlayState::Editing) return;
  const std::string selected = selection_ != kNoObject ? document_->node(selection_).name : "";
  if (!document_->redo()) return;
  refresh_preview();
  select_by_name(selected);
}

bool EditorSession::save(std::string& error) {
  if (!document_->save(error)) return false;
  loaded_write_time_ = write_time(document_->path());
  external_change_pending_ = false;
  log::info("saved " + document_->path().string());
  return true;
}

bool EditorSession::play(const InputSource& input, std::string& error, AudioSystem* audio) {
  if (play_state_ != PlayState::Editing) return true;
  EngineOptions options;
  options.game_dir = game_dir_;
  options.headless = true;
  options.scene = &document_->desc();
  options.input = &input;
  options.audio = audio;
  auto created = Engine::create(options);
  if (auto* message = std::get_if<std::string>(&created)) {
    error = *message;
    return false;
  }
  runtime_ = std::move(std::get<std::unique_ptr<Engine>>(created));
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
  const auto time = write_time(document_->path());
  if (time == loaded_write_time_ || play_state_ != PlayState::Editing) return false;
  // Each version of the file is handled once. A reload that fails because the file is still being
  // written is tried again when the writer finishes (the file's time changes again), without
  // logging the same error on every check in between.
  loaded_write_time_ = time;
  if (document_->dirty()) {
    external_change_pending_ = true;
    log::warning(document_->path().string() + " changed on disk while you have unsaved edits");
    return false;
  }
  return reload_from_disk();
}

bool EditorSession::reload_from_disk() {
  const std::filesystem::path path = document_->path();
  auto opened = SceneDocument::open(path);
  if (auto* error = std::get_if<std::string>(&opened)) {
    log::error("cannot reload " + path.string() + ": " + *error);
    return false;
  }
  const std::string selected = selection_ != kNoObject ? document_->node(selection_).name : "";
  document_.emplace(std::move(std::get<SceneDocument>(opened)));
  loaded_write_time_ = write_time(path);
  external_change_pending_ = false;
  refresh_preview();
  select_by_name(selected);
  log::info("reloaded " + path.string());
  return true;
}

}  // namespace redgem
