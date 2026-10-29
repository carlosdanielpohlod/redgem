// Engine: starting a game up, part by part.

#include "app/engine.hpp"

#include <string>
#include <vector>

#include "app/game_identity.hpp"
#include "app/input_bindings_store.hpp"
#include "app/save_location.hpp"
#include "app/scene_bodies.hpp"
#include "app/scene_water.hpp"
#include "app/scene_files.hpp"
#include "app/splash_screen.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "core/translations.hpp"
#include "storage/save_file.hpp"

namespace redgem {
namespace {

// What follows the word that a game's id cannot be used.
constexpr const char* kSavedByFolderNote = ". Until it is put right, the game's save goes by where its folder is";

// What game.json says that cannot be used, and what the game does instead.
void warn_of_unusable(const GameManifest& manifest) {
  if (!manifest.id_error.empty()) log::warning(manifest.id_error + kSavedByFolderNote);
  if (!manifest.script_budget_error.empty()) log::warning(manifest.script_budget_error);
  for (const std::string& warning : manifest.physics_warnings) log::warning(warning);
}

}  // namespace

Engine::Engine(const EngineOptions& options)
    : options_(options),
      images_dir_(options.game_dir / config::kImagesDirName),
      presenter_(options.presenter),
      clock_(config::kFixedDeltaSeconds, config::kMaxFrameSeconds, config::kMaxFixedStepsPerFrame),
      profiler_(options.profile_time),
      network_watch_(options.network_time) {}

Engine::~Engine() {
  scene_load_.reset();
  keep_bindings();
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
  const GameIdentityReading identity = begin_loading();
  const GameManifest manifest = read_game_manifest(options_.game_dir);
  warn_of_unusable(manifest);
  build_world(manifest.settings.collision_layers);
  open_presenter(manifest.settings);
  open_audio();
  open_saves();
  open_player_choices(manifest.settings);
  fill_context(manifest.settings.title);
  connect_display();
  take_identity(identity);
  if (serves_a_room()) network_.serve_room(options_.server_room);  // before any script asks
  start_scripts(std::chrono::milliseconds(manifest.settings.script_budget_ms));
  network_watch_.restart();  // the network's time starts with the game, not with its loading
}

void Engine::build_world(const CollisionLayers& layers) {
  collision_layers_ = layers;
  scene_ = Scene::from_desc(scene_desc_);
  animations_.reset(scene_desc_.animations, scene_);
  particles_.set_gravity(gravity());
  if (options_.random_seed) particles_.set_seed(*options_.random_seed);
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
  context_.measure_text = [this](const std::string& text, int size) {
    return presenter_->font_text_width(text, size, context_.ui_font, options_.game_dir / config::kFontsDirName);
  };
}

void Engine::open_audio() {
  if (options_.audio == nullptr) owned_audio_ = std::make_unique<NullAudio>();
  audio().set_sounds_dir(options_.game_dir / config::kSoundsDirName);
}

void Engine::open_display(const DisplaySettings& game_defaults) {
  context_.display = options_.display.value_or(game_defaults);
  follow_players_volume(context_.display.volume);
}

void Engine::follow_players_volume(float volume) {
  if (volume == players_volume_) return;
  players_volume_ = volume;
  audio().set_player_volume(volume);
}

void Engine::connect_display() {
  context_.apply_display = [this](const DisplaySettings& display) {
    presenter_->apply_display(display);
    follow_players_volume(display.volume);
  };
  if (keeps_display(options_.display_kept_in)) {
    context_.save_display = [this](const DisplaySettings& display, std::string& error) {
      return keep_display(*options_.display_kept_in, display, error);
    };
  }
  context_.display_modes = [this] { return presenter_->display_modes(); };
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

void Engine::open_player_choices(const GameSettings& settings) {
  open_display(settings.display);
  open_bindings();
}

void Engine::open_bindings() {
  std::vector<std::string> problems;
  input_.bindings().declare(read_game_input(options_.game_dir, problems));
  for (const std::string& problem : problems) log::warning(problem);
  bindings_file_ = chosen_bindings_file();
  if (!bindings_file_.empty()) read_player_bindings(bindings_file_, input_.bindings());
  kept_bindings_ = player_bindings_text(input_.bindings());
}

std::filesystem::path Engine::chosen_bindings_file() const {
  if (!options_.bindings_file.empty()) return options_.bindings_file;
  if (serves_a_room()) return {};  // (a room has no players of its own)
  return bindings_file_for(options_.game_dir);
}

void Engine::keep_bindings() {
  if (!bindings_changed_) return;
  bindings_changed_ = false;
  std::string text = player_bindings_text(input_.bindings());
  if (bindings_file_.empty() || text == kept_bindings_) return;
  if (!write_player_bindings_text(bindings_file_, text)) {
    log::warning("could not keep the players' bindings in " + bindings_file_.string());
    return;
  }
  kept_bindings_ = std::move(text);
}

void Engine::connect_effects() {
  context_.animations = &animations_;
  context_.particles = &particles_;
  context_.particle_emitters = &particle_emitters_;
  context_.renders_particles = options_.presenter != nullptr;
}

void Engine::fill_context(const std::string& game_title) {
  context_.save = options_.save != nullptr ? options_.save : save_.get();
  connect_effects();
  context_.network = &network_;
  context_.discovery = &discovery_;
  context_.http = &http_;
  context_.game_title = game_title;
  context_.scene = &scene_;
  context_.audio = &audio();
  context_.physics = physics_.get();
  context_.input = &input_;
  context_.time.fixed_delta = config::kFixedDeltaSeconds;
  context_.profiler = &profiler_;
  context_.scene_name = options_.scene_name;
  connect_context();
}

GameIdentityReading Engine::begin_loading() {
  GameIdentityReading identity = read_game_identity(options_.game_dir);
  show_splash(options_.presenter, options_.game_dir, identity.identity);  // (while the world is built)
  return identity;
}

void Engine::take_identity(const GameIdentityReading& identity) {
  if (!identity.error.empty()) log::warning(tr("game.identity.unusable", {{"error", identity.error}}));
  context_.game_version = identity.identity.version;
}

void Engine::connect_context() {
  context_.on_bindings_changed = [this] { bindings_changed_ = true; };
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

void Engine::start_scripts(std::chrono::milliseconds script_budget) {
  scripts_ = std::make_unique<ScriptHost>(context_, options_.game_dir / config::kScriptsDirName);
  scripts_->vm().set_budget(script_budget);
  seed_scripts();
  scripts_->attach_scripts();
  scripts_->run_scene_script(context_.scene_name);
}

void Engine::seed_scripts() {
  if (!options_.random_seed) return;
  mrb_state* mrb = scripts_->vm().state();
  scripts_->vm().call(mrb_top_self(mrb), "srand", {mrb_int_value(mrb, *options_.random_seed)});
}

AudioSystem& Engine::audio() { return options_.audio != nullptr ? *options_.audio : *owned_audio_; }

}  // namespace redgem
