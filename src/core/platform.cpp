#include "core/platform.hpp"

#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI   // raylib defines its own Rectangle
#define NOUSER  // ... and CloseWindow, ShowCursor
#include <windows.h>
#elif defined(__APPLE__)
#include <cstdint>
#include <cstring>
#include <mach-o/dyld.h>
#endif

namespace redgem::platform {
namespace {

constexpr const char* kWindowsExecutableExtension = ".exe";

#ifdef _WIN32
constexpr const char* kHomeVariable = "USERPROFILE";
constexpr unsigned long kMaxPathLength = 32768;  // Windows' longest path, in characters
#else
constexpr const char* kHomeVariable = "HOME";
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
constexpr const char* kSelfExecutable = "/proc/self/exe";
#endif

#ifdef _WIN32
// Whether a variable that is not to be overwritten already has a value.
bool keeps_its_value(const std::string& name, bool overwrite) {
  return !overwrite && std::getenv(name.c_str()) != nullptr;
}
#else
// Where a path really leads, past every link; empty when it leads nowhere.
std::filesystem::path real_location(const std::filesystem::path& path) {
  std::error_code error;
  const std::filesystem::path real = std::filesystem::canonical(path, error);
  return error ? std::filesystem::path() : real;
}
#endif

}  // namespace

#ifdef _WIN32
std::filesystem::path executable_path() {
  std::wstring buffer(kMaxPathLength, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  buffer.resize(length);
  return std::filesystem::path(buffer);
}
#elif defined(__APPLE__)
std::filesystem::path executable_path() {
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);  // (only says how long the path is)
  std::string path(size, '\0');
  if (_NSGetExecutablePath(path.data(), &size) != 0) return {};
  path.resize(std::strlen(path.c_str()));
  return real_location(path);
}
#else
std::filesystem::path executable_path() { return real_location(kSelfExecutable); }
#endif

std::string environment(const std::string& name) {
  const char* value = std::getenv(name.c_str());
  return value != nullptr ? value : "";
}

std::filesystem::path home_dir() {
  const std::string home = environment(kHomeVariable);
  return home.empty() ? std::filesystem::temp_directory_path() : std::filesystem::path(home);
}

void set_environment(const std::string& name, const std::string& value, bool overwrite) {
#ifdef _WIN32
  if (keeps_its_value(name, overwrite)) return;
  _putenv_s(name.c_str(), value.c_str());
#else
  setenv(name.c_str(), value.c_str(), overwrite ? 1 : 0);
#endif
}

std::string executable_file_name(const std::string& name, bool for_windows) {
  return for_windows ? name + kWindowsExecutableExtension : name;
}

}  // namespace redgem::platform
