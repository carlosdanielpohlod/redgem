#include "core/platform.hpp"

#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI   // raylib defines its own Rectangle
#define NOUSER  // ... and CloseWindow, ShowCursor
#include <windows.h>
#endif

namespace redgem::platform {
namespace {

constexpr const char* kWindowsExecutableExtension = ".exe";

#ifdef _WIN32
constexpr const char* kHomeVariable = "USERPROFILE";
constexpr unsigned long kMaxPathLength = 32768;  // Windows' longest path, in characters
#else
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

std::string executable_file_name(const std::string& name, bool for_windows) {
  return for_windows ? name + kWindowsExecutableExtension : name;
}

}  // namespace redgem::platform
