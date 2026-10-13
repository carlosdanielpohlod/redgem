// Part of EditorSession: Play mode, and the scene file on disk (saving it, and following what
// others write to it).

#include "editor/editor_session.hpp"

#include "core/config.hpp"
#include "core/files.hpp"
#include "core/log.hpp"

namespace redgem {
namespace {

std::filesystem::file_time_type write_time(const std::filesystem::path& path) {
  std::error_code error;
  const auto time = std::filesystem::last_write_time(path, error);
  return error ? std::filesystem::file_time_type{} : time;
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

bool EditorSession::save(std::string& error) {
  if (!document_.save(error)) return false;
  loaded_write_time_ = write_time(document_.path());
  external_change_pending_ = false;
  log::info("saved " + document_.path().string());
  return true;
}

bool EditorSession::play(const PlayDevices& devices, std::string& error) {
  if (!editing()) return true;
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
  if (editing()) return;
  play_state_ = paused ? PlayState::Paused : PlayState::Playing;
}

void EditorSession::stop() {
  if (editing()) return;
  runtime_.reset();
  // The game's music ends with the game, whoever ended it: Stop, or the game itself (Game.quit).
  if (game_audio_ != nullptr) game_audio_->stop_music();
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
  const std::string selected = selected_name();
  document_ = std::move(std::get<SceneDocument>(opened));
  loaded_write_time_ = write_time(path);
  external_change_pending_ = false;
  refresh_and_reselect(selected);
  log::info("reloaded " + path.string());
  return true;
}

}  // namespace redgem
