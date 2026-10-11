#include "editor/script_files.hpp"

#include "core/platform.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "core/config.hpp"
#include "core/log.hpp"
#include "script/script_naming.hpp"

namespace redgem {
namespace {

std::string strip_extension(const std::string& name) {
  const std::string extension = config::kRubyFileExtension;
  if (name.size() > extension.size() &&
      name.compare(name.size() - extension.size(), extension.size(), extension) == 0) {
    return name.substr(0, name.size() - extension.size());
  }
  return name;
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
  if (base.empty() || !std::islower(static_cast<unsigned char>(base[0]))) return false;
  for (const char c : base) {
    const auto u = static_cast<unsigned char>(c);
    if (!std::islower(u) && !std::isdigit(u) && c != '_') return false;
  }
  return true;
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

std::optional<std::string> read_text_file(const std::filesystem::path& path, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot read " + path.string();
    return std::nullopt;
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

bool write_text_file(const std::filesystem::path& path, const std::string& text, std::string& error) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out || !out.write(text.data(), static_cast<std::streamsize>(text.size()))) {
    error = "cannot write " + path.string();
    return false;
  }
  return true;
}

void open_in_system(const std::filesystem::path& path) {
  // (The desktop opens it: the command is the platform's own, with the path quoted.)
  const int status = std::system(platform::open_command(path).c_str());  // NOLINT(bugprone-command-processor)
  if (status != 0) log::warning("could not open " + path.string());
}

}  // namespace redgem
