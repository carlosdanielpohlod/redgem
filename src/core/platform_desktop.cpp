// What the desktops the engine's tools run on do each their own way: where a program keeps the
// user's data, and how a file is opened with the desktop's own application.

#include "core/platform.hpp"

#include <cstdlib>

#include "core/log.hpp"

namespace redgem::platform {
namespace {

constexpr const char* kConfigHomeVariable = "XDG_CONFIG_HOME";  // where the desktop keeps settings, when it says
constexpr const char* kConfigFolder = ".config";                // in the home folder, when it does not
constexpr const char* kUserDataFolder = "redgem";
// Where the Mac keeps what programs remember: the Library's Application Support, in the home folder.
constexpr const char* kMacLibraryFolder = "Library";
constexpr const char* kMacSupportFolder = "Application Support";
constexpr const char* kMacUserDataFolder = "RedGem";
// What opens a file with the desktop's application, on the desktops that are not Windows.
constexpr const char* kLinuxOpener = "xdg-open ";
constexpr const char* kMacOpener = "open ";
constexpr const char* kQuietInBackground = " >/dev/null 2>&1 &";  // detached, and quiet: its output is not interesting

#if defined(_WIN32)
constexpr Desktop kDesktop = Desktop::Windows;
#elif defined(__APPLE__)
constexpr Desktop kDesktop = Desktop::Mac;
#else
constexpr Desktop kDesktop = Desktop::Linux;
#endif

// The folder set_user_data_dir gave; empty until it gives one.
std::filesystem::path& given_user_data_dir() {
  static std::filesystem::path folder;
  return folder;
}

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

Desktop this_desktop() { return kDesktop; }

bool is_windows() { return kDesktop == Desktop::Windows; }

bool is_mac() { return kDesktop == Desktop::Mac; }

bool has_posix_shell() { return !is_windows(); }

std::filesystem::path user_data_dir_on(Desktop desktop, const std::filesystem::path& home,
                                       const std::string& config_home) {
  if (desktop == Desktop::Mac) return home / kMacLibraryFolder / kMacSupportFolder / kMacUserDataFolder;
  const std::filesystem::path settings = config_home.empty() ? home / kConfigFolder : std::filesystem::path(config_home);
  return settings / kUserDataFolder;
}

std::filesystem::path user_data_dir() {
  if (!given_user_data_dir().empty()) return given_user_data_dir();
  return user_data_dir_on(kDesktop, home_dir(), environment(kConfigHomeVariable));
}

void set_user_data_dir(const std::filesystem::path& folder) { given_user_data_dir() = folder; }

std::string posix_shell_word(const std::filesystem::path& text) { return quoted(text, '\'', "'\\''"); }

std::string open_command_on(Desktop desktop, const std::filesystem::path& path) {
  switch (desktop) {
    case Desktop::Windows:
      // `start` takes the first quoted argument as the window's title: give it an empty one.
      return "start \"\" " + quoted(path, '"', "\"\"");
    case Desktop::Mac:
      return kMacOpener + posix_shell_word(path) + kQuietInBackground;
    case Desktop::Linux:
      break;
  }
  return kLinuxOpener + posix_shell_word(path) + kQuietInBackground;
}

std::string open_command(const std::filesystem::path& path) { return open_command_on(kDesktop, path); }

void open_in_system(const std::filesystem::path& path) {
  // (The desktop opens it: the command is the platform's own, with the path quoted.)
  const int status = std::system(open_command(path).c_str());  // NOLINT(bugprone-command-processor)
  if (status != 0) log::warning("could not open " + path.string());
}

}  // namespace redgem::platform
