#include "core/platform.hpp"

#include <cstdlib>
#include <iostream>

#include "core/log.hpp"
#include "core/safe_path.hpp"

#ifdef __ANDROID__
#include <android/log.h>
#endif

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

#ifdef __ANDROID__
constexpr const char* kLogTag = "redgem";  // what the system's log files the engine's messages under
#else
constexpr const char* kLogPrefix = "[redgem] ";
#endif
constexpr char kLineBreak = '\n';
// What the lines of a message after its first start with: only a message starts with the mark.
constexpr const char* kContinuation = "\n  ";

#ifdef _WIN32
constexpr bool kWindows = true;
constexpr const char* kHomeVariable = "USERPROFILE";
constexpr unsigned long kMaxPathLength = 32768;  // Windows' longest path, in characters
#else
constexpr bool kWindows = false;
constexpr const char* kHomeVariable = "HOME";
constexpr const char* kSelfExecutable = "/proc/self/exe";
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

std::filesystem::path home_dir() {
  const char* home = std::getenv(kHomeVariable);
  return home != nullptr && *home != '\0' ? std::filesystem::path(home) : std::filesystem::temp_directory_path();
}

std::filesystem::path user_data_dir() {
  if (!given_user_data_dir().empty()) return given_user_data_dir();
  const char* config = std::getenv(kConfigHomeVariable);
  const std::filesystem::path settings =
      config != nullptr && *config != '\0' ? std::filesystem::path(config) : home_dir() / kConfigFolder;
  return settings / kUserDataFolder;
}

void set_user_data_dir(const std::filesystem::path& folder) { given_user_data_dir() = folder; }

void set_environment(const std::string& name, const std::string& value, bool overwrite) {
#ifdef _WIN32
  if (!overwrite && std::getenv(name.c_str()) != nullptr) return;
  _putenv_s(name.c_str(), value.c_str());
#else
  setenv(name.c_str(), value.c_str(), overwrite ? 1 : 0);
#endif
}

std::string open_command(const std::filesystem::path& path) {
#ifdef _WIN32
  // `start` takes the first quoted argument as the window's title: give it an empty one.
  return "start \"\" " + quoted(path, '"', "\"\"");
#else
  // Detached, and quiet: its output is not interesting.
  return "xdg-open " + quoted(path, '\'', "'\\''") + " >/dev/null 2>&1 &";
#endif
}

std::string log_text(const std::string& message) {
  std::string text;
  for (const char c : printable_lines(message)) {
    if (c == kLineBreak) {
      text += kContinuation;
    } else {
      text.push_back(c);
    }
  }
  return text;
}

#ifdef __ANDROID__
// An Android app has no terminal: its messages go to the system's log (adb logcat).
void write_log(log::Level level, const std::string& message) {
  const int priority = level == log::Level::Error     ? ANDROID_LOG_ERROR
                       : level == log::Level::Warning ? ANDROID_LOG_WARN
                       : level == log::Level::Info    ? ANDROID_LOG_INFO
                                                      : ANDROID_LOG_DEBUG;
  __android_log_write(priority, kLogTag, log_text(message).c_str());
}
#else
void write_log(log::Level level, const std::string& message) {
  std::ostream& out = (level == log::Level::Error || level == log::Level::Warning) ? std::cerr : std::cout;
  out << kLogPrefix << log::level_name(level) << ": " << log_text(message) << kLineBreak << std::flush;
}
#endif

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
