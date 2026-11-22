#include "project/mcp_terrain_tool.hpp"

#include <algorithm>
#include <regex>
#include <stdexcept>

#include "app/terrain_files.hpp"
#include "core/config.hpp"
#include "editor/document/scene_document.hpp"
#include "project/project_json_files.hpp"
#include "project/scene_command.hpp"
#include "project/scene_lookup.hpp"
#include "project/tool_commands.hpp"
#include "scene/ground/terrain_layers_file.hpp"
#include "scene/property.hpp"
#include "scene/props.hpp"

namespace redgem::tools::mcp {
namespace {

namespace fs = std::filesystem;
using Lines = std::vector<std::string>;

constexpr int kIndent = 2;
constexpr const char* kLayersExtension = ".json";
constexpr const char* kLayersName = "[a-z0-9_-]+";

std::invalid_argument wrong(const std::string& what) { return std::invalid_argument(what); }

std::string joined(const Lines& lines) {
  std::string text;
  for (const auto& line : lines) text += line + "\n";
  return text;
}

std::string checked_name(const std::string& name) {
  if (!std::regex_match(name, std::regex(kLayersName))) throw wrong("name must be small letters, digits, _ and -");
  return name;
}

// The layers files of the game, by name ("beach"), sorted.
Lines layers_files(const fs::path& game) {
  Lines names;
  std::error_code missing;
  for (const auto& file : fs::directory_iterator(game / config::kTerrainsDirName, missing)) {
    if (file.path().extension() == kLayersExtension) names.push_back(file.path().stem().string());
  }
  std::sort(names.begin(), names.end());
  return names;
}

// The name of the layers the scene's object names with its terrain_layers property.
std::string named_by_object(const Options& options, const Json& args) {
  std::string error;
  const auto file = named_scene_file(options.game, args.value("scene", std::string(config::kMainSceneName)), error);
  if (!file) throw wrong(error);
  auto opened = SceneDocument::open(*file);
  if (const auto* problem = std::get_if<std::string>(&opened)) throw wrong(*problem);
  const SceneDesc& desc = std::get<SceneDocument>(opened).desc();
  const FoundObject found = find_object(desc, args["object"].get<std::string>());
  if (found.index == kNoIndex) throw wrong(found.error);
  const auto name = get_string(desc.nodes[static_cast<std::size_t>(found.index)].props, props::kTerrainLayers);
  if (!name) throw wrong(args["object"].get<std::string>() + " names no layers: give it prop.terrain_layers=<name>");
  return *name;
}

std::string layers_name(const Options& options, const Json& args) {
  if (args.contains("object")) return named_by_object(options, args);
  return checked_name(args.value("name", std::string()));
}

// Whether the call asks about every layers file: it names neither a file nor an object.
bool lists_all(const Json& args) { return !args.contains("name") && !args.contains("object"); }

Json layers(const Options& options, const Json& args) {
  if (lists_all(args)) return tool_result(joined(layers_files(options.game)));
  const std::string name = layers_name(options, args);
  const fs::path file = options.game / terrain_layers_path(name);
  if (!fs::exists(file)) throw wrong("the game has no " + terrain_layers_path(name));
  return tool_result(terrain_layers_path(name) + ":\n" + project_files::read_object(file).dump(kIndent));
}

// What reading the layers file says is wrong with it, as the game reads it.
project_files::Findings layers_findings(const fs::path& game, const std::string& name) {
  std::string error;
  if (read_terrain_files(game, name, TerrainReading::Weights, error)) return {};
  return {{error}, {}};
}

// Whether a call would replace a file without saying it means to.
bool replaces_unconfirmed(const fs::path& file, const Json& args) {
  return fs::exists(file) && !args.value("confirm", false);
}

Json set_layers(const Options& options, const Json& args) {
  const std::string name = checked_name(args.value("name", std::string()));
  if (!args.contains("content")) throw wrong("set_layers needs content: the whole layers file, as JSON");
  const fs::path file = options.game / terrain_layers_path(name);
  if (replaces_unconfirmed(file, args))
    throw wrong(terrain_layers_path(name) + " is there; replacing it needs confirm=true");
  const Lines warnings =
      project_files::write_checked(file, args["content"], [&] { return layers_findings(options.game, name); });
  return tool_result("wrote " + terrain_layers_path(name) + "; paint an object that names it with action=paint" +
                     (warnings.empty() ? "" : "\n" + joined(warnings)));
}

Json paint(const Options& options, const Json& args) {
  if (!args.contains("object")) throw wrong("paint needs object: the terrain to paint");
  Lines words{kSceneCommand, "paint", options.game.string(), args["object"].get<std::string>()};
  if (args.contains("scene")) words.insert(words.end(), {"--scene", args["scene"].get<std::string>()});
  const auto result = run_tool_command(words);
  if (!result) throw std::logic_error("missing scene paint");
  return tool_result(result->out + result->err, result->exit_code != 0);
}

using Action = Json (*)(const Options&, const Json&);
struct Known {
  const char* name;
  Action run;
  bool reads;
};
constexpr Known kActions[] = {
    {"layers", layers, true},
    {"set_layers", set_layers, false},
    {"paint", paint, false},
};

const Known* action_named(const std::string& name) {
  const auto* found = std::find_if(std::begin(kActions), std::end(kActions),
                                   [&name](const Known& known) { return name == known.name; });
  return found == std::end(kActions) ? nullptr : found;
}

}  // namespace

Json terrain_tool(const Options& options, const Json& args) {
  const Known* action = action_named(args.at("action").get<std::string>());
  if (action == nullptr) throw wrong("unknown terrain action");
  if (options.read_only && !action->reads) throw wrong("read-only mode only reads the layers");
  return action->run(options, args);
}

bool reads_terrain(const std::string& action) {
  const Known* known = action_named(action);
  return known != nullptr && known->reads;
}

}  // namespace redgem::tools::mcp
