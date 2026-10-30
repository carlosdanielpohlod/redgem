// The editor's life: its window and what draws in it, from opening a game to closing.

#include "editor/editor_app.hpp"

#include <imgui.h>
#include <ImGuizmo.h>

#include "core/config.hpp"
#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/scene_viewport.hpp"
#include "editor/ui/interface.hpp"
#include "render/renderer.hpp"
#include "render/window.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr int kExitSuccess = 0;        // process exit code of a normal run
constexpr const char* kAnyPopup = "";  // with ImGuiPopupFlags_AnyPopupId: whichever popup is open

WindowSettings window_settings(GpuChoice gpu) {
  WindowSettings settings;
  settings.width = kWindowWidth;
  settings.height = kWindowHeight;
  settings.title = kWindowTitle;
  settings.resizable = true;
  settings.gpu = gpu;
  return settings;
}

}  // namespace

std::variant<std::unique_ptr<EditorApp>, std::string> EditorApp::create(const std::filesystem::path& game_dir,
                                                                        EditorOptions options) {
  std::unique_ptr<EditorApp> app(new EditorApp());
  app->builds_ = std::move(options.builds);
  app->console_ = std::make_unique<ConsoleLog>();
  auto opened = EditorSession::open(game_dir);
  if (auto* error = std::get_if<std::string>(&opened)) return *error;
  app->session_ = std::move(std::get<std::unique_ptr<EditorSession>>(opened));
  app->scripts_ = std::make_unique<ScriptWorkspace>(game_dir / config::kScriptsDirName);
  app->open_window(options.gpu, std::move(options.clock));
  app->make_agent_panel(std::move(options.agent));
  log::info("editing " + app->session_->document().path().string());
  return app;
}

// The window, and what draws and sounds in it, for the game just opened.
void EditorApp::open_window(GpuChoice gpu, FrameClock clock) {
  window_ = std::make_unique<Window>(window_settings(gpu));
  clock_ = clock ? std::move(clock) : [this] { return window_->frame_seconds(); };
  renderer_ = std::make_unique<Renderer>(*window_);
  renderer_->upload(session_->scene_desc());
  renderer_->set_images_dir(session_->game_dir() / config::kImagesDirName);
  viewport_ = std::make_unique<SceneViewport>();
  audio_ = create_raylib_audio();
  interface_ = std::make_unique<ui::Interface>(*window_, ui::Interface::Layout::Dockable);
  look_through_main_camera();
  rescan_files();
}

void EditorApp::look_through_main_camera() {
  const Scene& scene = session_->scene();
  const ObjectId main_camera = scene.main_camera();
  if (main_camera == kNoObject) return;
  const Transform world = scene.world_transform(main_camera);
  camera_.look_at(world.position, world.position + world.forward());
}

EditorApp::~EditorApp() {
  agent_panel_.reset();  // its agent's tools talk to this editor
  session_.reset();  // the running game, if any, goes before who shows it
  game_view_.reset();
  viewport_.reset();  // GPU resources go before the window
  interface_.reset();
  renderer_.reset();
  window_.reset();
}

bool EditorApp::close_requested() const { return window_->close_requested(); }

void EditorApp::rescan_files() { files_ = ProjectFiles::scan(session_->game_dir()); }

int EditorApp::run() {
  while (!finished()) {
    // raylib reports the close button for a single frame, so remember it.
    if (close_requested()) request_exit(EditorExit::Quit);
    frame();
  }
  return kExitSuccess;
}

bool EditorApp::editing_blocked() const {
  // No structural edits or undo while a drag or a dialog is in progress.
  return ImGuizmo::IsUsing() || ImGui::IsAnyItemActive() || ImGui::IsPopupOpen(kAnyPopup, ImGuiPopupFlags_AnyPopupId);
}

bool EditorApp::unsaved_work() const { return session_->document().dirty() || scripts_->any_modified(); }

ui::PanelContext EditorApp::panel_context() {
  ui::PanelContext context{*session_, *scripts_, files_, panel_state_, spawn_point(), audio_.get()};
  context.terrain = &terrain_tool_;
  return context;
}

float EditorApp::viewport_width() const { return viewport_->width_pixels(); }
float EditorApp::viewport_height() const { return viewport_->height_pixels(); }

}  // namespace redgem
