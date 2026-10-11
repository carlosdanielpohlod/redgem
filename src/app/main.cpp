#include <chrono>
#include <thread>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "net/relay_server.hpp"
#include "core/exit_codes.hpp"
#include "app/cli.hpp"
#include "app/engine.hpp"
#include "app/game_project.hpp"
#include "app/user_settings.hpp"
#include "editor/editor_app.hpp"
#include "editor/project_hub.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"

namespace {

constexpr int kExitUsage = 2;
constexpr float kRelayPollSeconds = 0.005f;  // between looks at the relay's connections

// The game to open: the one given, else the one picked in the project hub (which shows why the
// last one did not open). False when the hub was closed instead.
bool pick_game(std::filesystem::path& game, std::string& hub_message) {
  using namespace redgem;
  if (!game.empty()) {
    remember_game(recent_games_file(), game);
    return true;
  }
  ProjectHub hub({default_games_folder(), recent_games_file()});
  hub.show_error(hub_message);
  hub_message.clear();
  const auto chosen = hub.run();
  if (chosen) game = *chosen;
  return chosen.has_value();
}

// One game in the editor, until the editor is left. The exit code, or nullopt to go (back) to the
// hub: a broken game from the command line exits; from the hub it returns there.
std::optional<int> edit_game(const std::filesystem::path& game, bool from_command_line, std::string& hub_message) {
  using namespace redgem;
  auto created = EditorApp::create(game);
  if (auto* error = std::get_if<std::string>(&created)) {
    log::error(*error);
    if (from_command_line) return kExitFailure;
    hub_message = "Cannot open " + game.filename().string() + ": " + *error;
    return std::nullopt;
  }
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  const int code = app.run();
  if (app.exit_action() != EditorExit::OpenHub) return code;
  return std::nullopt;
}

// The editor, with the project hub when no game is given (and for File > New / Open Game).
int run_editor(const redgem::CliOptions& options) {
  std::filesystem::path game = options.game_dir;
  bool from_command_line = !game.empty();
  std::string hub_message;  // why the last game did not open, shown in the hub
  while (pick_game(game, hub_message)) {
    if (const auto code = edit_game(game, from_command_line, hub_message)) return *code;
    from_command_line = false;
    game.clear();  // back to the hub
  }
  return redgem::kExitSuccess;
}

void report_rooms(const redgem::net::RelayServer& relay, std::size_t& rooms) {
  using namespace redgem;
  if (relay.room_count() == rooms) return;
  rooms = relay.room_count();
  log::info(std::to_string(rooms) + " room(s) open, " + std::to_string(relay.connection_count()) + " machine(s)");
}

[[noreturn]] void serve_until_stopped(redgem::net::RelayServer& relay) {
  std::size_t rooms = 0;
  auto before = std::chrono::steady_clock::now();
  for (;;) {
    const auto now = std::chrono::steady_clock::now();
    relay.poll(std::chrono::duration<float>(now - before).count());
    before = now;
    report_rooms(relay, rooms);
    std::this_thread::sleep_for(std::chrono::duration<float>(kRelayPollSeconds));
  }
}

// A meeting point for games over the internet (see RelayServer): runs until it is stopped.
int run_relay(const redgem::CliOptions& options) {
  using namespace redgem;
  net::RelayServer relay;
  std::string error;
  const auto wanted = static_cast<std::uint16_t>(options.relay_port != 0 ? options.relay_port : net::kDefaultRelayPort);
  if (!relay.start(wanted, error)) {
    log::error("cannot listen on port " + std::to_string(wanted) + ": " + error);
    return kExitFailure;
  }
  log::info("relay listening on port " + std::to_string(relay.port()) + " (Ctrl+C stops it)");
  serve_until_stopped(relay);
}

int play(const redgem::EngineOptions& options) {
  using namespace redgem;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) {
    log::error(*error);
    return kExitFailure;
  }
  return std::get<std::unique_ptr<Engine>>(created)->run();
}

int play_game(const redgem::CliOptions& options) {
  redgem::EngineOptions engine;
  engine.game_dir = options.game_dir;
  engine.screenshot_dir = options.screenshot_dir;
  return play(engine);
}

int show_help(const redgem::CliOptions&) {
  std::cout << redgem::usage();
  return redgem::kExitSuccess;
}

int new_game(const redgem::CliOptions& options) {
  using namespace redgem;
  std::string error;
  if (!create_game(options.game_dir, options.template_name, error)) {
    log::error(error);
    return kExitFailure;
  }
  std::cout << "created " << options.game_dir.string() << " (" << options.template_name << "). Open it with: redgem "
            << options.game_dir.string() << "\n";
  return kExitSuccess;
}

// The system a build is for: the one named on the command line (parse_cli took only names that
// exist), else the one this program runs on.
redgem::BuildTarget target_of(const redgem::CliOptions& options) {
  const auto named = redgem::build_target_from_name(options.build_target);
  return named ? *named : redgem::host_build_target();
}

int build(const redgem::CliOptions& options) {
  using namespace redgem;
  std::string error;
  const BuildTarget target = target_of(options);
  const auto executable = build_game_for(options.game_dir, options.build_dir, target, error);
  if (!executable) {
    log::error(error);
    return kExitFailure;
  }
  // (An Android app is not run from here: it is installed on a phone.)
  const char* next = target == BuildTarget::Android ? ": install it with adb install " : ": run ";
  std::cout << "built " << options.build_dir.string() << " for " << build_target_name(target) << next
            << executable->string() << "\n";
  return kExitSuccess;
}

// What each way of running the program does.
struct Mode {
  redgem::RunMode mode;
  int (*run)(const redgem::CliOptions&);
};
constexpr Mode kModes[] = {
    {redgem::RunMode::Help, show_help},  {redgem::RunMode::NewGame, new_game},  {redgem::RunMode::BuildGame, build},
    {redgem::RunMode::Relay, run_relay}, {redgem::RunMode::Editor, run_editor}, {redgem::RunMode::Game, play_game},
};

int run_mode(const redgem::CliOptions& options) {
  for (const Mode& known : kModes) {
    if (known.mode == options.mode) return known.run(options);
  }
  return redgem::kExitFailure;
}

// Whatever goes wrong while running is said, and fails the run.
template <typename Run>
int reporting_errors(Run run) {
  try {
    return run();
  } catch (const std::exception& error) {
    redgem::log::error(error.what());
    return redgem::kExitFailure;
  }
}

// A built game: started by itself, the executable plays the game that ships next to it.
std::optional<int> play_bundled_game(const std::vector<std::string>& args) {
  using namespace redgem;
  if (!args.empty()) return std::nullopt;
  const auto game = bundled_game(platform::executable_path());
  if (!game) return std::nullopt;
  apply_gpu_choice(std::nullopt);
  EngineOptions options;
  options.game_dir = *game;
  return reporting_errors([&options] { return play(options); });
}

// What the command line asks for, done. (Reading the line is inside the same net as the running:
// nothing a user types ends the program without a word.)
int run_command_line(const std::vector<std::string>& args) {
  using namespace redgem;
  const CliResult cli = parse_cli(args);
  if (!cli.ok) {
    std::cerr << cli.error << "\n\n" << usage();
    return kExitUsage;
  }
  apply_gpu_choice(cli.options.gpu);
  return run_mode(cli.options);
}

}  // namespace

int main(int argc, char** argv) {
  const std::vector<std::string> args(argv + 1, argv + argc);
  if (const auto code = play_bundled_game(args)) return *code;
  return reporting_errors([&args] { return run_command_line(args); });
}
