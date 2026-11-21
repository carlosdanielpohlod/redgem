#include "project/mcp_build_tool.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>

#include "core/log.hpp"
#include "editor/build_job.hpp"
#include "editor/editor_logic.hpp"
#include "project/build_steps.hpp"
#include "project/game_build.hpp"
#include "project/game_templates.hpp"

namespace redgem::tools::mcp {
namespace {

namespace fs = std::filesystem;

constexpr auto kWaitStep = std::chrono::milliseconds(100);
constexpr int kStepsPerSecond = 10;         // waits of kWaitStep in a second
constexpr std::size_t kMessagesShown = 20;  // the last lines a build logged, in its answer

std::invalid_argument wrong(const std::string& what) { return std::invalid_argument(what); }

// The one build of this process, and how the last one ended, once status took it.
struct Builds {
  BuildJob job;
  std::optional<BuildOutcome> last;
  std::string step_told;  // the step status last put in the events
};

Builds& builds() {
  static Builds state;
  return state;
}

BuildTarget target_of(const Json& args) {
  if (!args.contains("target")) return host_build_target();
  const auto target = build_target_from_name(args["target"].get<std::string>());
  if (!target) throw wrong("target must be one of: linux, windows, android, macos");
  return *target;
}

AndroidFormat format_of(const Json& args, BuildTarget target) {
  const bool bundle = args.value("format", std::string("apk")) == "bundle";
  if (bundle && target != BuildTarget::Android) throw wrong("format=bundle is for target=android only");
  return bundle ? AndroidFormat::Bundle : AndroidFormat::Apk;
}

GameLayout layout_of(const Json& args) { return args.value("loose", false) ? GameLayout::Loose : GameLayout::Packed; }

fs::path game_folder(const Options& options) { return fs::weakly_canonical(fs::absolute(options.game)); }

// Where it goes: next to the game, as the editor's Build puts it, or in the folder named there.
fs::path out_folder(const Options& options, const Json& args, BuildTarget target) {
  const fs::path game = game_folder(options);
  const std::string name =
      args.value("folder", editor_logic::build_folder_name(game.filename().string(), build_target_name(target),
                                                           target == host_build_target()));
  if (!valid_game_name(name)) throw wrong("folder must be a folder name: letters, digits, spaces, - and _");
  return game.parent_path() / name;
}

BuildChoice choice_for(BuildTarget target, AndroidFormat format, GameLayout layout) {
  BuildChoice choice;
  choice.name = build_target_name(target);
  choice.for_this_system = target == host_build_target();
  choice.build = [target, format, layout](const fs::path& game_dir, const fs::path& out_dir,
                                          const BuildProgress& progress, std::string& error) {
    const build_steps::Listen listen(progress);
    return build_game_for(game_dir, out_dir, target, error, format, layout);
  };
  return choice;
}

Json just_ended(BuildOutcome outcome);

// A build that ended and was not asked about yet becomes the last one, told in the events.
void settle() {
  if (auto ended = builds().job.take_outcome()) just_ended(std::move(*ended));
}

Json start(const Options& options, const Json& args) {
  const BuildTarget target = target_of(args);
  BuildRequest request{choice_for(target, format_of(args, target), layout_of(args)), game_folder(options),
                       out_folder(options, args, target)};
  const fs::path out = request.out_dir;
  settle();
  if (builds().job.busy())
    throw wrong("a build for " + builds().job.target() + " runs; wait for it with action=status");
  if (!builds().job.start(std::move(request))) throw std::runtime_error("cannot start a build: no thread for it");
  builds().step_told.clear();
  log::info("building for " + std::string(build_target_name(target)) + " into " + out.string());
  return tool_result("building for " + std::string(build_target_name(target)) + " into " + out.string() +
                     " from the saved files; follow it with action=status (wait up to 60 s a call)");
}

// The build's step, put in the events once each.
void tell_step(const std::string& step) {
  if (step.empty() || step == builds().step_told) return;
  builds().step_told = step;
  log::info("build: " + step);
}

// How the running build ended, waiting up to `seconds` for it (telling its steps on the way);
// nullopt while it still runs.
std::optional<BuildOutcome> wait_for_end(int seconds) {
  for (int left = seconds * kStepsPerSecond;; --left) {
    tell_step(builds().job.step());
    auto outcome = builds().job.take_outcome();
    if (outcome || left <= 0) return outcome;
    std::this_thread::sleep_for(kWaitStep);
  }
}

// What the build logged, into this process's events, and its last lines for the answer.
std::string messages_of(const BuildOutcome& outcome) {
  std::string text;
  const std::size_t first = outcome.messages.size() > kMessagesShown ? outcome.messages.size() - kMessagesShown : 0;
  for (std::size_t i = 0; i < outcome.messages.size(); ++i) {
    log::info("build: " + outcome.messages[i].text);
    if (i >= first) text += outcome.messages[i].text + "\n";
  }
  return text;
}

std::string ended_text(const BuildOutcome& outcome) {
  const std::string took = " (" + elapsed_text(outcome.seconds) + ")";
  if (const auto& made = outcome.made) return "built for " + outcome.target + took + ": " + made->string() + "\n";
  return "the build for " + outcome.target + " failed" + took + ": " + outcome.error + "\n";
}

// A build that ended now: told once in the events, then kept as the last one.
Json just_ended(BuildOutcome outcome) {
  const std::string text = ended_text(outcome) + messages_of(outcome);
  log::info(ended_text(outcome));
  const bool failed = !outcome.succeeded();
  builds().last = std::move(outcome);
  return tool_result(text, failed);
}

Json running() {
  tell_step(builds().job.step());
  const std::string step = builds().job.step().empty() ? "starting" : builds().job.step();
  return tool_result("building for " + builds().job.target() + ": " + step + " (" +
                     elapsed_text(builds().job.seconds()) + ")");
}

Json last_build() {
  const auto& last = builds().last;
  if (!last) return tool_result("no build was started; start one with action=start");
  return tool_result("last build: " + ended_text(*last), !last->succeeded());
}

Json status(const Json& args) {
  if (!builds().job.busy()) return last_build();
  auto ended = wait_for_end(args.value("wait", 0));
  return ended ? just_ended(std::move(*ended)) : running();
}

}  // namespace

Json build_tool(const Options& options, const Json& args) {
  const std::string action = args.at("action");
  if (action == "start") return start(options, args);
  return status(args);
}

}  // namespace redgem::tools::mcp
