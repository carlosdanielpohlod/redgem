#include "app/cli.hpp"

#include "app/game_project.hpp"
#include "app/user_settings.hpp"
#include "core/config.hpp"

namespace redgem {

std::string usage() {
  return "usage:\n"
         "  redgem                            open the project hub (new / recent games)\n"
         "  redgem <game_dir>                 open the editor\n"
         "  redgem <game_dir> --play          play the game\n"
         "  redgem <game_dir> --test <system_test.rb> [--screenshots <dir>] [--headless]\n"
         "  redgem --ruby-test <test_file.rb>\n"
         "  redgem new <game_dir> [--template empty|platformer]   create a game\n"
         "  redgem build <game_dir> <out_dir>                     make a standalone copy\n"
         "\n"
         "  --gpu auto|dedicated|integrated      graphics card for this run (default: the saved setting,\n"
         "                                       else the dedicated card when there is one)\n"
         "\n"
         "  <game_dir> holds " + std::string(config::kSceneFileName) + " (exported from Blender) and a " +
         config::kScriptsDirName + "/ folder.\n";
}

CliResult parse_cli(const std::vector<std::string>& args) {
  CliResult result;
  CliOptions& options = result.options;
  options.screenshot_dir = config::kDefaultScreenshotDir;

  auto fail = [&result](const std::string& message) {
    result.ok = false;
    result.error = message;
    return result;
  };

  // What to do is said once: by a subcommand or by one of the mode flags.
  bool mode_chosen = false;
  bool conflict = false;
  auto choose = [&](RunMode mode) {
    conflict = conflict || (mode_chosen && options.mode != mode);
    options.mode = mode;
    mode_chosen = true;
  };
  std::size_t first = 0;
  if (!args.empty() && (args[0] == "new" || args[0] == "build")) {
    choose(args[0] == "new" ? RunMode::NewGame : RunMode::BuildGame);
    first = 1;
  }
  for (std::size_t i = first; i < args.size(); ++i) {
    const std::string& arg = args[i];
    auto value = [&](std::string& out) {
      // The next argument, unless it is missing or is itself an option.
      if (i + 1 >= args.size() || (!args[i + 1].empty() && args[i + 1][0] == '-')) return false;
      out = args[++i];
      return true;
    };
    std::string text;
    if (arg == "--help" || arg == "-h") {
      result.options = CliOptions{};
      result.options.mode = RunMode::Help;  // help wins over anything else on the line
      result.ok = true;
      return result;
    } else if (arg == "--play") {
      choose(RunMode::Game);
    } else if (arg == "--test") {
      if (!value(text)) return fail("--test needs a file");
      options.test_script = text;
      choose(RunMode::Game);
    } else if (arg == "--ruby-test") {
      if (!value(text)) return fail("--ruby-test needs a file");
      choose(RunMode::RubyTests);
      options.ruby_test_file = text;
    } else if (arg == "--screenshots") {
      if (!value(text)) return fail("--screenshots needs a directory");
      options.screenshot_dir = text;
    } else if (arg == "--template") {
      if (!value(text)) return fail("--template needs a name");
      options.template_name = text;
    } else if (arg == "--gpu") {
      if (!value(text) || !gpu_choice_from_name(text)) return fail("--gpu needs auto, dedicated or integrated");
      options.gpu = text;
    } else if (arg == "--headless") {
      options.headless = true;
      choose(RunMode::Game);
    } else if (!arg.empty() && arg[0] == '-') {
      return fail("unknown option " + arg);
    } else if (options.game_dir.empty()) {
      options.game_dir = normalized_folder(arg);
    } else if (options.mode == RunMode::BuildGame && options.build_dir.empty()) {
      options.build_dir = normalized_folder(arg);
    } else {
      return fail("unexpected argument " + arg);
    }
  }
  if (conflict) return fail("choose one of: new, build, --play, --test, --ruby-test");
  // Without a window nobody can close the game: only a test ends such a run.
  if (options.headless && !options.test_script) return fail("--headless needs --test <system_test.rb>");
  const bool needs_game = options.mode == RunMode::Game || options.mode == RunMode::NewGame ||
                          options.mode == RunMode::BuildGame;
  if (needs_game && options.game_dir.empty()) return fail("missing <game_dir>");
  if (options.mode == RunMode::BuildGame && options.build_dir.empty()) return fail("missing <out_dir>");
  result.ok = true;
  return result;
}

}  // namespace redgem
