#include "project/mcp_tools.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <stdexcept>
#include <vector>

#include "core/event_journal.hpp"
#include "core/message_lines.hpp"
#include "core/safe_path.hpp"
#include "core/short_text.hpp"
#include "project/mcp_game_tools.hpp"
#include "project/tool_commands.hpp"
#include "project/editor_bridge.hpp"

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
  Json asked = args;
  if (name == "scene_list" && !asked.contains("limit")) asked["limit"] = kDefaultListLimit;
  Words words = scene_words(options, asked, scene_action(name));
  for (const char* key : {"depth", "limit"}) option(words, asked, key);
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

// One line per event: its first line, led by the place in the game's files it points at, with the
// game's folder left out (a backtrace's absolute paths cost tokens and say nothing new).
Json event_row(const JournalEvent& event, const std::filesystem::path& game) {
  const std::string compact = compact_message(event.text, game);
  const std::string text(utf8_prefix(compact, kEventTextBytes));
  return {{"id", event.id},
          {"level", log::level_name(event.level)},
          {"text", text},
          {"truncated", text.size() != compact.size()}};
}

Json events(const Options& options, const Json& args) {
  const auto page = log::events_since(args.value("since", std::uint64_t(0)), args.value("limit", kDefaultEvents),
                                      level(args.value("level", std::string("debug"))));
  Json rows = Json::array();
  for (const auto& event : page.events) rows.push_back(event_row(event, options.game));
  return {{"events", rows},
          {"cursor", page.cursor},
          {"dropped", page.dropped},
          {"more", page.more},
          {"cursor_ahead", page.cursor_ahead}};
}

// What a page says about itself after its cursor, only when there is something to say.
std::string page_notes(const Json& data) {
  std::string notes;
  if (data["more"].get<bool>()) notes += " more=true (call again with this since)";
  if (data["dropped"].get<std::uint64_t>() > 0) notes += " dropped=" + data["dropped"].dump();
  if (data["cursor_ahead"].get<bool>()) notes += " cursor_ahead=true (since is newer than this process)";
  return notes;
}

// The cursor to pass as since next time first, so that no cut of a long page can lose it, then
// "#2 error scripts/coin.rb:4: ..." lines.
std::string events_text(const Json& data) {
  std::string text = "cursor=" + data["cursor"].dump() + page_notes(data) + "\n";
  for (const auto& row : data["events"])
    text += "#" + row["id"].dump() + " " + row["level"].get<std::string>() + " " + row["text"].get<std::string>() +
            (row["truncated"].get<bool>() ? "..." : "") + "\n";
  if (data["events"].empty()) text += "no new events\n";
  return text;
}

Json native_result(const Options& options, const std::string& name, const Json& args) {
  if (name == "events") return tool_result(events_text(events(options, args)));
  if (name == "game_shot") return game_shot(options, args);
  if (name == "game_query") return game_query(options, args);
  const auto result = run_tool_command(command(options, name, args));
  if (!result) throw std::logic_error("missing native command");
  return tool_result(result->out + result->err, result->exit_code != kExitSuccess);
}
// The tools a connected editor answers, about its unsaved document and its own process.
bool answered_live(const std::string& name) {
  return name == "editor" || name == "events" || name == "game_query" || name.rfind("scene_", 0) == 0;
}

constexpr const char* kNoEditor =
    "no editor of this game is running with its bridge; start one with `redgem <game> --editor-bridge`";
// The first line of an answer the saved files gave because no editor took the call.
constexpr const char* kFromFiles = "(no editor is running: answered from the saved files)\n";
// How long discovery leaves alone a connection whose listener did not prove itself: a program
// that took a crashed editor's port then costs one handshake wait a minute, not one a call.
constexpr auto kUnprovenBackoff = std::chrono::seconds(60);

using Clock = std::chrono::steady_clock;

// Connection files that did not prove themselves lately, with when (this process's memory).
std::map<std::filesystem::path, Clock::time_point>& unproven() {
  static std::map<std::filesystem::path, Clock::time_point> files;
  return files;
}

bool backing_off(const std::filesystem::path& file) {
  const auto found = unproven().find(file);
  return found != unproven().end() && Clock::now() - found->second < kUnprovenBackoff;
}

// What a miss teaches about a connection file: a refused one is gone for good, an unproven one
// is left alone for a while.
void remember(const std::filesystem::path& file, EditorMiss miss) {
  if (miss == EditorMiss::Refused) forget_connection(file);
  if (miss == EditorMiss::Unproven) unproven()[file] = Clock::now();
}

// The answer of the files, said to be theirs, so that nobody takes it for the open editor's.
Json from_files(Json reply) {
  for (auto& part : reply["content"]) {
    if (part.value("type", "") != "text") continue;
    part["text"] = kFromFiles + part["text"].get<std::string>();
    return reply;
  }
  reply["content"].insert(reply["content"].begin(), Json{{"type", "text"}, {"text", kFromFiles}});
  return reply;
}

// The first running editor of the game answers; without one, the files do (the editor's own
// actions cannot be done without it).
Json discovered(const Options& options, const std::string& name, const Json& args) {
  for (const auto& file : editor_connections(options.game, options.bridge_root)) {
    if (backing_off(file)) continue;
    auto attempt = try_editor(options.game, file, name, args);
    if (attempt.reply) return *attempt.reply;
    remember(file, attempt.miss);
  }
  if (name == "editor") return tool_result(kNoEditor, true);
  return from_files(native_result(options, name, args));
}
}  // namespace

Json invoke(const Options& options, const std::string& name, const Json& args) {
  const Json tools = catalog(options);
  const auto tool = std::find_if(tools.begin(), tools.end(), [&](const Json& item) { return item["name"] == name; });
  if (tool == tools.end()) throw std::invalid_argument("unknown or disabled tool");
  validate_arguments(tool->at("inputSchema"), args);
  if (options.editor_connection.empty() || !answered_live(name)) return native_result(options, name, args);
  if (discovers_editor(options)) return discovered(options, name, args);
  return call_editor(options.game, options.editor_connection, name, args);
}
}  // namespace redgem::tools::mcp
