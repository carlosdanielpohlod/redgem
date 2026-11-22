#include "project/mcp_terrain_tool.hpp"

#include <algorithm>
#include <stdexcept>

#include "app/terrain_files.hpp"
#include "core/safe_path.hpp"
#include "core/config.hpp"
#include "editor/document/scene_document.hpp"
#include "project/project_json_files.hpp"
#include "project/scene_command.hpp"
#include "project/scene_lookup.hpp"
#include "project/tool_commands.hpp"
#include "scene/ground/terrain_layers.hpp"
#include "scene/ground/terrain_layers_file.hpp"
#include "scene/property.hpp"
#include "scene/props.hpp"

namespace redgem::tools::mcp {
namespace {

namespace fs = std::filesystem;
using Lines = std::vector<std::string>;

constexpr int kIndent = 2;
constexpr const char* kLayersExtension = ".json";
constexpr std::uintmax_t kMaxLayersFileBytes = 1024u * 1024u;  // as the game reads one (app/terrain_files.cpp)

std::invalid_argument wrong(const std::string& what) { return std::invalid_argument(what); }

std::string joined(const Lines& lines) {
  std::string text;
  for (const auto& line : lines) text += line + "\n";
  return text;
}

// Whether a name can be a layers file of terrains/ itself, as the game takes one.
bool layers_file_name(const std::string& name) { return safe_asset_name(name) && name.find('/') == std::string::npos; }

std::string checked_name(const std::string& name) {
  if (!layers_file_name(name)) throw wrong("the layers' name must be a file name of terrains/, not " + name);
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
  return checked_name(*name);
}

// Whether a call names its layers twice: by name and by the object that names them.
bool names_both(const Json& args) { return args.contains("name") && args.contains("object"); }

std::string layers_name(const Options& options, const Json& args) {
  if (names_both(args)) throw wrong("give name or object, not both");
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
  if (fs::file_size(file) > kMaxLayersFileBytes) throw wrong(terrain_layers_path(name) + " is larger than 1 MiB");
  return tool_result(terrain_layers_path(name) + ":\n" + project_files::read_object(file).dump(kIndent));
}

// The layers `content` declares, read as the game reads a layers file; throws what is wrong.
TerrainLayers declared(const Json& content) {
  TerrainLayers layers;
  const std::string error = read_terrain_layers(content, layers);
  if (!error.empty()) throw wrong("not written, as the game could not read it: " + error);
  return layers;
}

Lines layer_names(const TerrainLayers& layers) {
  Lines names;
  names.reserve(layers.layers.size());
  for (const auto& layer : layers.layers) names.push_back(layer.name);
  return names;
}

// The layer names of the file there now (none when there is none, or it cannot be read).
Lines layer_names_of(const fs::path& file) {
  try {
    return layer_names(declared(project_files::read_object(file)));
  } catch (const std::exception&) {
    return {};
  }
}

// Weights painted for other layers (or in another order) no longer fit: they go, to be painted again.
std::string drop_stale_weights(const Options& options, const std::string& name) {
  std::error_code ignored;
  for (int picture = 0; picture < kSplatPictures; ++picture)
    fs::remove(options.game / splat_picture_path(name, picture), ignored);
  return "; its layers changed, so its weights were taken away: paint it again with action=paint";
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
  const Lines before = layer_names_of(file);
  const Lines after = layer_names(declared(args["content"]));
  project_files::write_checked(file, args["content"], [] { return project_files::Findings{}; });
  const bool stale = !before.empty() && before != after;
  return tool_result("wrote " + terrain_layers_path(name) + (stale ? drop_stale_weights(options, name) : "") +
                     "; paint an object that names it with action=paint");
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
