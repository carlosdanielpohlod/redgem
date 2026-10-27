#include "project/mcp_tools.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "core/event_journal.hpp"
#include "core/safe_path.hpp"
#include "core/short_text.hpp"
#include "project/tool_commands.hpp"

namespace redgem::tools::mcp {
namespace {
constexpr int kDefaultRunFrames = 60;
constexpr std::size_t kDefaultEvents = 20;
constexpr std::size_t kEventTextBytes = 256;
using Words = std::vector<std::string>;

void option(Words& words, const Json& args, const char* key) {
  if (!args.contains(key)) return;
  words.push_back(std::string("--") + key);
  words.push_back(args[key].is_string() ? args[key].get<std::string>() : args[key].dump());
}

std::string inside(const Options& options, const Json& args, const char* key) {
  const auto path = file_inside(options.game, args.at(key).get<std::string>());
  if (!path)
    throw std::invalid_argument(std::string(key) + " must name a regular file inside the game, without symlinks");
  return path->string();
}

Words scene_words(const Options& options, const Json& args, const char* action) {
  Words words{"scene", action, options.game.string()};
  if (args.contains("object")) words.push_back(args["object"]);
  option(words, args, "scene");
  return words;
}

Words edit_words(const Options& options, const Json& args) {
  const std::string action = args.at("action");
  if (action == "remove" && !args.value("confirm", false)) throw std::invalid_argument("remove needs confirm=true");
  if (action != "add" && (args.contains("from") || args.contains("parent")))
    throw std::invalid_argument("from and parent are only accepted by add");
  const auto assignments = args.value("assignments", Words{});
  if (action == "remove" && !assignments.empty()) throw std::invalid_argument("remove does not take assignments");
  Words words = scene_words(options, args, action.c_str());
  words.insert(words.end(), assignments.begin(), assignments.end());
  option(words, args, "from");
  option(words, args, "parent");
  return words;
}

Words run_words(const Options& options, const Json& args) {
  Words words{"run", options.game.string(), "--frames", std::to_string(args.value("frames", kDefaultRunFrames))};
  for (const char* key : {"scene", "seed", "assert", "until", "print", "every"}) option(words, args, key);
  if (args.contains("input")) {
    words.push_back("--input");
    words.push_back(inside(options, args, "input"));
  }
  return words;
}

Words api_words(const Json& args) {
  if (args.contains("class") && args.contains("search")) throw std::invalid_argument("choose class or search");
  Words words{"api"};
  if (args.contains("class")) words.push_back(args["class"]);
  option(words, args, "search");
  return words;
}

const char* scene_action(const std::string& name) {
  if (name == "scene_list") return "list";
  if (name == "scene_get") return "get";
  return "diff";
}

Words scene_command(const Options& options, const std::string& name, const Json& args) {
  Words words = scene_words(options, args, scene_action(name));
  for (const char* key : {"depth", "limit"}) option(words, args, key);
  if (name == "scene_diff") {
    words.push_back("--against");
    words.push_back(inside(options, args, "against"));
  }
  return words;
}

Words command(const Options& options, const std::string& name, const Json& args) {
  if (name == "scene_edit") return edit_words(options, args);
  if (name == "game_run") return run_words(options, args);
  if (name == "game_check") return {"check", options.game.string()};
  if (name == "api") return api_words(args);
  return scene_command(options, name, args);
}

log::Level level(const std::string& name) {
  if (name == "error") return log::Level::Error;
  if (name == "warning") return log::Level::Warning;
  if (name == "info") return log::Level::Info;
  return log::Level::Debug;
}

Json event_row(const JournalEvent& event) {
  const std::string text(utf8_prefix(event.text, kEventTextBytes));
  return {{"id", event.id},
          {"level", log::level_name(event.level)},
          {"text", text},
          {"truncated", text.size() != event.text.size()}};
}

Json events(const Json& args) {
  const auto page = log::events_since(args.value("since", std::uint64_t(0)), args.value("limit", kDefaultEvents),
                                      level(args.value("level", std::string("debug"))));
  Json rows = Json::array();
  for (const auto& event : page.events) rows.push_back(event_row(event));
  return {{"events", rows},
          {"cursor", page.cursor},
          {"dropped", page.dropped},
          {"more", page.more},
          {"cursor_ahead", page.cursor_ahead}};
}
}  // namespace

Json invoke(const Options& options, const std::string& name, const Json& args) {
  const Json tools = catalog(options);
  const auto tool = std::find_if(tools.begin(), tools.end(), [&](const Json& item) { return item["name"] == name; });
  if (tool == tools.end()) throw std::invalid_argument("unknown or disabled tool");
  validate_arguments(tool->at("inputSchema"), args);
  if (name == "events") {
    const Json data = events(args);
    Json reply = tool_result(data.dump());
    reply["structuredContent"] = data;
    return reply;
  }
  const auto result = run_tool_command(command(options, name, args));
  if (!result) throw std::logic_error("missing native command");
  return tool_result(result->out + result->err, result->exit_code != kExitSuccess);
}
}  // namespace redgem::tools::mcp
