// Where a program named by the user (an agent's command) is on this machine.

#include <sstream>
#include <system_error>

#include "core/platform.hpp"
#include "editor/agent/child_process.hpp"

namespace redgem::agent {
namespace {

namespace fs = std::filesystem;

constexpr const char* kPathVariable = "PATH";
#ifdef _WIN32
constexpr char kPathSeparator = ';';
constexpr const char* kExtensionsVariable = "PATHEXT";
constexpr const char* kDefaultExtensions = ".COM;.EXE;.BAT;.CMD";
#else
constexpr char kPathSeparator = ':';
#endif

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::stringstream stream(text);
  std::string part;
  while (std::getline(stream, part, separator)) {
    if (!part.empty()) parts.push_back(part);
  }
  return parts;
}

// A name with a folder in it ("./agent", "/usr/bin/agent") is looked for there alone.
bool names_a_folder(const std::string& program) { return fs::path(program).has_parent_path(); }

// The first of `base` + each extension that can be started.
fs::path with_an_extension(const fs::path& base, const std::vector<std::string>& extensions,
                           const ProgramTest& runnable) {
  for (const std::string& extension : extensions) {
    fs::path candidate = base;
    candidate += extension;
    if (runnable(candidate)) return candidate;
  }
  return {};
}

#ifdef _WIN32
std::vector<std::string> this_systems_extensions() {
  const std::string listed = platform::environment(kExtensionsVariable);
  std::vector<std::string> extensions{""};
  for (const std::string& extension : split(listed.empty() ? kDefaultExtensions : listed, kPathSeparator)) {
    extensions.push_back(extension);
  }
  return extensions;
}

bool runnable_here(const fs::path& file) {
  std::error_code error;
  return fs::is_regular_file(file, error);
}
#else
std::vector<std::string> this_systems_extensions() { return {""}; }

bool runnable_here(const fs::path& file) {
  std::error_code error;
  const fs::perms owner_runs = fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
  return fs::is_regular_file(file, error) && (fs::status(file, error).permissions() & owner_runs) != fs::perms::none;
}
#endif

}  // namespace

fs::path find_program_in(const std::string& program, const std::string& search_path, char separator,
                         const std::vector<std::string>& extensions, const ProgramTest& runnable) {
  if (program.empty()) return {};
  if (names_a_folder(program)) return with_an_extension(program, extensions, runnable);
  for (const std::string& folder : split(search_path, separator)) {
    fs::path found = with_an_extension(fs::path(folder) / program, extensions, runnable);
    if (!found.empty()) return found;
  }
  return {};
}

fs::path find_program(const std::string& program) {
  return find_program_in(program, platform::environment(kPathVariable), kPathSeparator, this_systems_extensions(),
                         runnable_here);
}

}  // namespace redgem::agent
