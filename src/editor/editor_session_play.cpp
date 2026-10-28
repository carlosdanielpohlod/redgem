// Part of EditorSession: Play mode, and the scene files on disk (opening them, saving them, and
// following what others write to the one edited).

#include "editor/editor_session.hpp"

#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "core/log.hpp"
#include "editor/game_scenes.hpp"

namespace redgem {
namespace {

std::filesystem::file_time_type write_time(const std::filesystem::path& path) {
  std::error_code error;
  const auto time = std::filesystem::last_write_time(path, error);
  return error ? std::filesystem::file_time_type{} : time;
}

// A game running inside the editor: the edited scene instead of the file, with the editor's devices.
EngineOptions runtime_options(const std::filesystem::path& game_dir, const SceneDesc& scene,
                              const std::string& scene_name, const PlayDevices& devices) {
  EngineOptions options;
  options.game_dir = game_dir;
  options.scene = &scene;
  options.scene_name = scene_name;
  options.input = &devices.input;
  options.audio = devices.audio;
  options.presenter = devices.presenter;
  return options;
}

// The editor's audio after a game: its music ended, and back to what it was before the game
// changed its volume and pitch.
void end_game_audio(AudioSystem& audio) {
  audio.stop_music();
  audio.set_music_pitch(kAsRecorded);
  audio.set_music_volume(kFullVolume);
  audio.set_master_volume(kFullVolume);
  audio.set_player_volume(kFullVolume);
}

// Whether a copy of the document saved as `file` still finds the files next to it its images and
// buffers name: it names none, or it stays in the same folder.
bool finds_its_files(const SceneDocument& document, const std::filesystem::path& file) {
  return document.linked_files().empty() || file.parent_path() == document.path().parent_path();
}

}  // namespace

std::variant<std::unique_ptr<EditorSession>, std::string> EditorSession::open(const std::filesystem::path& folder) {
  const std::filesystem::path game_dir = normalized_folder(folder);
  const std::filesystem::path scene_path = game_dir / config::kSceneFileName;
  auto opened = SceneDocument::open(scene_path);
  if (auto* error = std::get_if<std::string>(&opened)) return *error;
  std::unique_ptr<EditorSession> session(new EditorSession(game_dir, std::move(std::get<SceneDocument>(opened))));
  session->begin_editing();
  return session;
}

void EditorSession::begin_editing() {
  scene_name_ = scene_name_of(game_dir_, document_.path());
  selection_.clear();
  loaded_write_time_ = write_time(document_.path());
  external_change_pending_ = false;
  refresh_preview();
}

bool EditorSession::open_scene(const std::string& name, std::string& error) {
  if (!editing()) {
    error = "stop the game to open another scene";
    return false;
  }
  auto opened = SceneDocument::open(scene_file(game_dir_, name));
  if (auto* message = std::get_if<std::string>(&opened)) {
    error = *message;
    return false;
  }
  document_ = std::move(std::get<SceneDocument>(opened));
  begin_editing();
  log::info("editing " + document_.path().string());
  return true;
}

void EditorSession::note_saved() {
  loaded_write_time_ = write_time(document_.path());
  external_change_pending_ = false;
  log::info("saved " + document_.path().string());
}

bool EditorSession::save(std::string& error) {
  if (!document_.save(error)) return false;
  note_saved();
  return true;
}

bool EditorSession::save_as(const std::string& name, std::string& error) {
  const auto file = ready_new_scene_file(game_dir_, name, error);
  if (!file) return false;
  if (!files_found_from(*file, error)) return false;
  if (!document_.save_as(*file, error)) return false;
  scene_name_ = name;
  note_saved();
  return true;
}

bool EditorSession::files_found_from(const std::filesystem::path& file, std::string& error) const {
  if (finds_its_files(document_, file)) return true;
  error = document_.path().filename().string() + " uses files next to it (" + document_.linked_files().front() +
          "): to save it in another folder, export it from Blender as a .glb with its textures in it";
  return false;
}

bool EditorSession::play(const PlayDevices& devices, std::string& error) {
  if (!editing()) return true;
  auto created = Engine::create(runtime_options(game_dir_, document_.desc(), scene_name(), devices));
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
  if (editing()) return;
  play_state_ = paused ? PlayState::Paused : PlayState::Playing;
}

void EditorSession::stop() {
  if (editing()) return;
  runtime_.reset();
  // The game's sound ends with the game, whoever ended it: Stop, or the game itself (Game.quit).
  if (game_audio_ != nullptr) end_game_audio(*game_audio_);
  game_audio_ = nullptr;
  play_state_ = PlayState::Editing;
  refresh_preview();
  log::info("stop");
}

void EditorSession::tick(float frame_seconds) {
  if (!playing()) return;
  runtime_->frame(frame_seconds);
  if (runtime_->finished()) stop();  // a script called Game.quit
}

bool EditorSession::step() {
  if (play_state_ != PlayState::Paused || !runtime_) return false;
  runtime_->frame(config::kFixedDeltaSeconds);
  if (runtime_->finished()) stop();
  return true;
}

bool EditorSession::reload_if_changed_on_disk() {
  const auto time = write_time(document_.path());
  if (!reload_wanted(time)) return false;
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
  const std::vector<std::string> selected = selected_names();
  document_ = std::move(std::get<SceneDocument>(opened));
  loaded_write_time_ = write_time(path);
  external_change_pending_ = false;
  refresh_and_reselect(selected);
  log::info("reloaded " + path.string());
  return true;
}

}  // namespace redgem
