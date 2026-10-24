// Engine: starting a game up, part by part.

#include "app/engine.hpp"

#include <string>

#include "app/save_location.hpp"
#include "app/scene_bodies.hpp"
#include "app/scene_water.hpp"
#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "storage/save_file.hpp"

namespace redgem {
namespace {

// What follows the word that a game's id cannot be used.
constexpr const char* kSavedByFolderNote = ". Until it is put right, the game's save goes by where its folder is";

}  // namespace

Engine::Engine(const EngineOptions& options)
    : options_(options),
      images_dir_(options.game_dir / config::kImagesDirName),
      presenter_(options.presenter),
      clock_(config::kFixedDeltaSeconds, config::kMaxFrameSeconds, config::kMaxFixedStepsPerFrame),
      network_watch_(options.network_time) {}

Engine::~Engine() {
  // Scripts reference the scene and physics through the context: tear them down first.
  scripts_.reset();
  physics_.reset();
}

std::variant<std::unique_ptr<Engine>, std::string> Engine::create(const EngineOptions& options) {
  if (options.scene != nullptr) return create(options, SceneDesc(*options.scene));
  auto scene = read_game_scene(options.game_dir);
  if (auto* error = std::get_if<std::string>(&scene)) return *error;
  return create(options, std::move(std::get<SceneDesc>(scene)));
}

std::unique_ptr<Engine> Engine::create(const EngineOptions& options, SceneDesc scene) {
  std::unique_ptr<Engine> engine(new Engine(options));
  engine->scene_desc_ = std::move(scene);
  engine->initialize();
  return engine;
}

void Engine::initialize() {
  build_world();
  const GameManifest manifest = read_game_manifest(options_.game_dir);
  if (!manifest.id_error.empty()) log::warning(manifest.id_error + kSavedByFolderNote);
  open_presenter(manifest.settings);
  open_audio();
  open_saves();
  fill_context(manifest.settings.title);
  if (serves_a_room()) network_.serve_room(options_.server_room);  // before any script asks
  start_scripts();
  network_watch_.restart();  // the network's time starts with the game, not with its loading
}

void Engine::build_world() {
  scene_ = Scene::from_desc(scene_desc_);
  animations_.reset(scene_desc_.animations, scene_);
  particles_.set_gravity(gravity());
  water_.reset(scene_desc_);
  make_bodies();
}

void Engine::open_presenter(const GameSettings& settings) {
  if (presenter_ == nullptr) {
    // Shown by nobody, the game is as big as its settings say.
    own_presenter_ = std::make_unique<NullPresenter>(SurfaceSize{settings.window_width, settings.window_height});
    presenter_ = own_presenter_.get();
  }
  const SurfaceSize size = presenter_->surface_size();
  context_.screen_width = size.width;
  context_.screen_height = size.height;
  context_.measure_text = [this](const std::string& text, int size) { return presenter_->text_width(text, size); };
}

void Engine::open_audio() {
  if (options_.audio == nullptr) owned_audio_ = std::make_unique<NullAudio>();
  audio().set_sounds_dir(options_.game_dir / config::kSoundsDirName);
}

void Engine::open_saves() {
  if (options_.save != nullptr) return;  // its runner's
  // A server's room with no store of the server's: what it saves lasts for the match.
  if (serves_a_room()) {
    save_ = std::make_unique<SaveFile>();
    return;
  }
  save_ =
      std::make_unique<SaveFile>(options_.save_file.empty() ? save_file_for(options_.game_dir) : options_.save_file);
}

void Engine::fill_context(const std::string& game_title) {
  context_.save = options_.save != nullptr ? options_.save : save_.get();
  context_.animations = &animations_;
  context_.particles = &particles_;
  context_.network = &network_;
  context_.discovery = &discovery_;
  context_.http = &http_;
  context_.game_title = game_title;
  context_.scene = &scene_;
  context_.audio = &audio();
  context_.physics = physics_.get();
  context_.input = &input_;
  context_.time.fixed_delta = config::kFixedDeltaSeconds;
  context_.scene_name = options_.scene_name;
  connect_context();
}

void Engine::connect_context() {
  context_.on_transform_written = [this](ObjectId id, TransformWrite write) {
    bodies::follow_write(scene_, *physics_, id, write);
    if (water_teleport_write(*physics_, id, write)) water_.forget(scene_.handle(id));
  };
  context_.spawn = [this](ObjectId source, const std::optional<Transform>& world, bool physics) {
    return spawn(source, world, physics);
  };
  context_.scene_exists = [this](const std::string& name) {
    return safe_scene_name(name) && std::filesystem::exists(scene_file(options_.game_dir, name));
  };
}

void Engine::start_scripts() {
  scripts_ = std::make_unique<ScriptHost>(context_, options_.game_dir / config::kScriptsDirName);
  scripts_->attach_scripts();
  scripts_->run_scene_script(context_.scene_name);
}

AudioSystem& Engine::audio() { return options_.audio != nullptr ? *options_.audio : *owned_audio_; }

}  // namespace redgem
