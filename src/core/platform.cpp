#include "core/platform.hpp"

#include <cstdlib>

#include "core/log.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI   // raylib defines its own Rectangle
#define NOUSER  // ... and CloseWindow, ShowCursor
#include <windows.h>
#endif

namespace redgem::platform {
namespace {

constexpr const char* kWindowsExecutableExtension = ".exe";
constexpr const char* kConfigHomeVariable = "XDG_CONFIG_HOME";  // where the desktop keeps settings, when it says
constexpr const char* kConfigFolder = ".config";                // in the home folder, when it does not
constexpr const char* kUserDataFolder = "redgem";

// The folder set_user_data_dir gave; empty until it gives one.
std::filesystem::path& given_user_data_dir() {
  static std::filesystem::path folder;
  return folder;
}

#ifdef _WIN32
constexpr bool kWindows = true;
constexpr const char* kHomeVariable = "USERPROFILE";
constexpr unsigned long kMaxPathLength = 32768;  // Windows' longest path, in characters
#else
constexpr bool kWindows = false;
constexpr const char* kHomeVariable = "HOME";
constexpr const char* kSelfExecutable = "/proc/self/exe";
#endif

#ifdef _WIN32
// Whether a variable that is not to be overwritten already has a value.
bool keeps_its_value(const std::string& name, bool overwrite) {
  return !overwrite && std::getenv(name.c_str()) != nullptr;
}
#endif

// Wraps a path in quotes for the shell, escaping the quotes inside it.
std::string quoted(const std::filesystem::path& path, char quote, const char* escaped_quote) {
  std::string out(1, quote);
  for (const char c : path.string()) {
    if (c == quote) {
      out += escaped_quote;
    } else {
      out.push_back(c);
    }
  }
  out.push_back(quote);
  return out;
}

}  // namespace

std::filesystem::path executable_path() {
#ifdef _WIN32
  std::wstring buffer(kMaxPathLength, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  buffer.resize(length);
  return std::filesystem::path(buffer);
#else
  std::error_code error;
  const std::filesystem::path self = std::filesystem::canonical(kSelfExecutable, error);
  return error ? std::filesystem::path() : self;
#endif
}

std::string environment(const std::string& name) {
  const char* value = std::getenv(name.c_str());
  return value != nullptr ? value : "";
}

std::filesystem::path home_dir() {
  const std::string home = environment(kHomeVariable);
  return home.empty() ? std::filesystem::temp_directory_path() : std::filesystem::path(home);
}

std::filesystem::path user_data_dir() {
  if (!given_user_data_dir().empty()) return given_user_data_dir();
  const std::string config = environment(kConfigHomeVariable);
  const std::filesystem::path settings = config.empty() ? home_dir() / kConfigFolder : std::filesystem::path(config);
  return settings / kUserDataFolder;
}

void set_user_data_dir(const std::filesystem::path& folder) { given_user_data_dir() = folder; }

void set_environment(const std::string& name, const std::string& value, bool overwrite) {
#ifdef _WIN32
  if (keeps_its_value(name, overwrite)) return;
  _putenv_s(name.c_str(), value.c_str());
#else
  setenv(name.c_str(), value.c_str(), overwrite ? 1 : 0);
#endif
}

std::string posix_shell_word(const std::filesystem::path& text) { return quoted(text, '\'', "'\\''"); }

std::string open_command(const std::filesystem::path& path) {
#ifdef _WIN32
  // `start` takes the first quoted argument as the window's title: give it an empty one.
  return "start \"\" " + quoted(path, '"', "\"\"");
#else
  // Detached, and quiet: its output is not interesting.
  return "xdg-open " + posix_shell_word(path) + " >/dev/null 2>&1 &";
#endif
}

bool is_windows() { return kWindows; }

bool has_posix_shell() { return !kWindows; }

void open_in_system(const std::filesystem::path& path) {
  // (The desktop opens it: the command is the platform's own, with the path quoted.)
  const int status = std::system(open_command(path).c_str());  // NOLINT(bugprone-command-processor)
  if (status != 0) log::warning("could not open " + path.string());
}

std::string executable_file_name(const std::string& name, bool for_windows) {
  return for_windows ? name + kWindowsExecutableExtension : name;
}

}  // namespace redgem::platform
