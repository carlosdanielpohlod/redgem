#include "project/mcp_import_settings_tool.hpp"

#include <optional>
#include <stdexcept>

#include "core/config.hpp"
#include "core/import_settings.hpp"
#include "core/safe_path.hpp"
#include "project/mcp_tools.hpp"

namespace redgem::tools::mcp {
namespace {

namespace fs = std::filesystem;
namespace settings = import_settings;

constexpr const char* kFieldIndent = "  ";
constexpr const char* kNeedsEditor =
    "apply imports a model again in the editor's open scene: start one with `redgem <game> --editor-bridge` and "
    "call again. The settings are saved already; the editor uses them the next time it imports the model";

std::string asset_name(const Json& args) { return args.at("asset").get<std::string>(); }

fs::path asset_file(const fs::path& game, const Json& args) {
  const auto file = file_inside(game, asset_name(args));
  if (!file)
    throw std::invalid_argument("asset must name a file inside the game, without symlinks: " + asset_name(args));
  return *file;
}

settings::Settings read_settings(const fs::path& file) {
  std::string error;
  auto read = settings::read_checked(file, error);
  if (!read) throw std::invalid_argument(error);
  return *read;
}

std::string choices_of(const settings::Field& field) {
  std::string text;
  for (const std::string& choice : field.choices) text += (text.empty() ? " (" : ", ") + choice;
  return text.empty() ? text : text + ")";
}

// "model models/oak.glb" then a line a field: "  up_axis = y (y, z)".
std::string listing(const std::string& asset, const settings::Settings& values) {
  std::string text = std::string(settings::kind_word(settings::kind_of(values))) + " " + asset + "\n";
  for (const settings::Field& field : settings::fields(values))
    text += kFieldIndent + field.name + " = " + field.value + choices_of(field) + "\n";
  return text;
}

// A field's default, as text: what null puts back.
std::string default_value(const settings::Settings& values, const std::string& name) {
  for (const settings::Field& field : settings::fields(settings::defaults(settings::kind_of(values))))
    if (field.name == name) return field.value;
  throw std::invalid_argument("unknown field: " + name);
}

// A value as set() reads it: a string as it is, a number or a switch as JSON writes it.
std::string value_text(const settings::Settings& values, const std::string& name, const Json& value) {
  if (value.is_null()) return default_value(values, name);
  if (value.is_string()) return value.get<std::string>();
  if (value.is_number() || value.is_boolean()) return value.dump();
  throw std::invalid_argument(name + ": must be text, a number, true, false or null");
}

void assign(settings::Settings& values, const Json& fields) {
  if (fields.empty()) throw std::invalid_argument("set needs fields, e.g. {\"scale\": 0.01}");
  for (const auto& [name, value] : fields.items()) {
    std::string error;
    if (settings::set(values, name, value_text(values, name, value), error)) continue;
    error.insert(0, name + ": ");
    throw std::invalid_argument(error);
  }
}

// What set says after saving: the fields, the file, and for a model how its copies get them.
std::string saved_text(const std::string& asset, const settings::Settings& values) {
  std::string text = listing(asset, values) + "saved " + asset + settings::kSidecarSuffix + "\n";
  if (settings::kind_of(values) == settings::Kind::Model)
    text += "objects imported from it before keep the old settings until import_settings action=apply\n";
  return text;
}

Json set_settings(const fs::path& file, const Json& args) {
  if (!args.contains("fields")) throw std::invalid_argument("set needs fields, e.g. {\"scale\": 0.01}");
  settings::Settings values = read_settings(file);
  assign(values, args["fields"]);  // (all of them, or none: nothing is saved before)
  std::string error;
  if (!settings::save(file, values, error)) throw std::runtime_error(error);
  return tool_result(saved_text(asset_name(args), values));
}

void refuse_misplaced_fields(const std::string& action, const Json& args) {
  if (action != "set" && args.contains("fields")) throw std::invalid_argument("fields is only accepted by set");
}

// Whether `relative` (the asset's path from models/) stays in models/, and the file is a model.
bool model_of_models(const fs::path& relative, const fs::path& file) {
  const bool in_models = !relative.empty() && *relative.begin() != "..";
  return in_models && settings::kind_of(file) == settings::Kind::Model;
}

}  // namespace

bool reads_import_settings(const std::string& action) { return action == "settings"; }

Json import_settings_files(const fs::path& game, const Json& args) {
  const std::string action = args.at("action");
  refuse_misplaced_fields(action, args);
  const fs::path file = asset_file(game, args);
  if (action == "set") return set_settings(file, args);
  if (action != "settings") throw std::invalid_argument(kNeedsEditor);
  return tool_result(listing(asset_name(args), read_settings(file)));
}

Json import_settings_tool(const Options& options, const Json& args) {
  return import_settings_files(options.game, args);
}

std::string model_to_reimport(const fs::path& game, const Json& args) {
  refuse_misplaced_fields("apply", args);
  const fs::path file = asset_file(game, args);  // (inside the game, through no link)
  const fs::path relative = fs::path(asset_name(args)).lexically_normal().lexically_relative(config::kModelsDirName);
  if (!model_of_models(relative, file))
    throw std::invalid_argument("apply takes a model of models/ (.glb or .gltf): " + asset_name(args));
  return relative.generic_string();
}

}  // namespace redgem::tools::mcp
