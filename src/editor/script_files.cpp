#include "editor/script_files.hpp"

#include <algorithm>
#include <cctype>

#include "core/config.hpp"
#include "core/files.hpp"
#include "script/script_naming.hpp"

namespace redgem {
namespace {

// Whether a name is something followed by `extension`.
bool has_extension(const std::string& name, const std::string& extension) {
  return name.size() > extension.size() &&
         name.compare(name.size() - extension.size(), extension.size(), extension) == 0;
}

std::string strip_extension(const std::string& name) {
  const std::string extension = config::kRubyFileExtension;
  return has_extension(name, extension) ? name.substr(0, name.size() - extension.size()) : name;
}

bool starts_lowercase(const std::string& name) {
  return !name.empty() && std::islower(static_cast<unsigned char>(name[0])) != 0;
}

// What a script's name is made of: lowercase letters, digits and _.
bool name_character(char c) {
  const auto u = static_cast<unsigned char>(c);
  return std::islower(u) != 0 || std::isdigit(u) != 0 || c == '_';
}

// What a new script holds after its first two lines (a comment with its file name and the class).
constexpr const char* kTemplateBody =
    "  # Blender/editor custom properties are available with prop(:name, default).\n"
    "  SPEED = 1.0\n"
    "\n"
    "  # Runs once, before the first update.\n"
    "  def start\n"
    "  end\n"
    "\n"
    "  # Runs every frame. dt is the frame time in seconds.\n"
    "  def update(dt)\n"
    "  end\n"
    "\n"
    "  # Other callbacks: fixed_update(dt), on_trigger_enter(other), on_trigger_exit(other),\n"
    "  # on_collision_enter(other), on_collision_exit(other), on_destroy.\n"
    "end\n";

}  // namespace

bool valid_script_name(const std::string& name) {
  const std::string base = strip_extension(name);
  if (!starts_lowercase(base)) return false;
  return std::all_of(base.begin(), base.end(), name_character);
}

std::string script_file_name(const std::string& name) { return strip_extension(name) + config::kRubyFileExtension; }

std::string script_template(const std::string& file_name) {
  return "# " + file_name + " - attach it to an object in the editor (Inspector > Scripts).\n" + "class " +
         class_name_for_script(file_name) + " < Behaviour\n" + kTemplateBody;
}

std::optional<std::string> create_script(const std::filesystem::path& scripts_dir, const std::string& name,
                                         std::string& error) {
  if (!valid_script_name(name)) {
    error = "use lowercase letters, digits and underscores, starting with a letter (e.g. enemy_ai)";
    return std::nullopt;
  }
  std::string file_name = script_file_name(name);
  const std::filesystem::path path = scripts_dir / file_name;
  if (std::filesystem::exists(path)) {
    error = file_name + " already exists";
    return std::nullopt;
  }
  std::error_code fs_error;
  std::filesystem::create_directories(scripts_dir, fs_error);
  if (!write_text_file(path, script_template(file_name), error)) return std::nullopt;
  return file_name;
}

}  // namespace redgem
