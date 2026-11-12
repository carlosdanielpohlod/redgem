#include "project/android/android_stage_cache.hpp"

#include <memory>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#else
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "project/android/android_stage_folder.hpp"

namespace redgem::android_stage {
namespace {
namespace fs = std::filesystem;

constexpr const char* kCacheFolder = "android-stage";

#ifdef _WIN32
fs::path local_app_data() {
  PWSTR name = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &name))) return {};
  const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> owned(name, CoTaskMemFree);
  return fs::path(owned.get());
}

bool private_cache(const fs::path& cache) {
  std::error_code problem;
  fs::create_directories(cache.parent_path(), problem);
  if (problem) return false;
  make_private_directory(cache, problem);
  return fs::is_directory(fs::symlink_status(cache, problem));
}
#else
constexpr mode_t kOtherWrite = S_IWGRP | S_IWOTH;
constexpr mode_t kOtherAccess = S_IRWXG | S_IRWXO;

// System-owned sticky directories (such as /tmp) protect their owned children from replacement.
bool protected_ancestor(const struct stat& info) {
  const bool sticky_system_dir = info.st_uid == 0 && (info.st_mode & S_ISVTX) != 0;
  return (info.st_mode & kOtherWrite) == 0 || sticky_system_dir;
}

bool existing_or_new(const fs::path& path, struct stat& info) {
  if (::lstat(path.c_str(), &info) == 0) return true;
  if (errno != ENOENT) return false;
  std::error_code problem;
  make_private_directory(path, problem);
  return ::lstat(path.c_str(), &info) == 0;
}

// The path has no symlinks, and no other user can replace any of its components.
class CachePath {
 public:
  bool safe_component(const fs::path& path, bool final) {
    struct stat info{};
    if (!existing_or_new(path, info) || !S_ISDIR(info.st_mode)) return false;
    const bool owned = info.st_uid == user_;
    if (final) return owned && (info.st_mode & kOtherAccess) == 0;
    const bool trusted_system = !user_tree_ && info.st_uid == 0;
    user_tree_ |= owned;
    return (owned || trusted_system) && protected_ancestor(info);
  }

 private:
  uid_t user_ = ::geteuid();
  bool user_tree_ = false;
};

bool private_cache(const fs::path& cache) {
  CachePath permissions;
  fs::path current;
  for (const auto& component : cache) {
    current /= component;
    if (!permissions.safe_component(current, current == cache)) return false;
  }
  return true;
}
#endif

}  // namespace

fs::path cache_root_on(platform::Desktop desktop, const fs::path& home, const fs::path& named) {
  if (desktop == platform::Desktop::Windows) return named.is_absolute() ? named / "RedGem" / kCacheFolder : fs::path{};
  if (!home.is_absolute()) return {};
  if (desktop == platform::Desktop::Mac) return home / "Library" / "Caches" / "RedGem" / kCacheFolder;
  const auto base = named.is_absolute() ? named : home / ".cache";
  return base / "redgem" / kCacheFolder;
}

fs::path cache_root() {
#ifdef _WIN32
  return cache_root_on(platform::this_desktop(), {}, local_app_data());
#else
  return cache_root_on(platform::this_desktop(), platform::environment_path("HOME"),
                       platform::environment_path("XDG_CACHE_HOME"));
#endif
}

bool ensure_cache(const fs::path& cache, std::string& error) {
  if (!cache.is_absolute()) {
    error = "cannot find a user cache for Android builds";
    return false;
  }
  if (private_cache(cache)) return true;
  error = "cannot use private Android build cache " + cache.string();
  return false;
}

}  // namespace redgem::android_stage
