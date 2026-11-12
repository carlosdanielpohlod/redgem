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

const char* private_cache_problem(const fs::path& cache, fs::path& failed) {
  failed = cache.parent_path();
  std::error_code problem;
  fs::create_directories(failed, problem);
  if (problem) return "cannot create cache parent";
  failed = cache;
  make_private_directory(cache, problem);
  return fs::is_directory(fs::symlink_status(cache, problem)) ? nullptr : "expected a real directory";
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

bool trusted_owner(uid_t owner) { return owner == 0 || owner == ::geteuid(); }

const char* leaf_problem(const struct stat& info) {
  if (info.st_uid != ::geteuid()) return "cache must belong to the current user";
  if ((info.st_mode & kOtherAccess) != 0) return "cache grants group or other access";
  return nullptr;
}

const char* ancestor_problem(const struct stat& info) {
  if (!trusted_owner(info.st_uid)) return "directory belongs to another user";
  if (!protected_ancestor(info)) return "directory allows other users to write";
  return nullptr;
}

const char* component_problem(const fs::path& path, bool final) {
  struct stat info{};
  if (!existing_or_new(path, info)) return "cannot inspect or create directory";
  if (S_ISLNK(info.st_mode)) return "unexpected symbolic link";
  if (!S_ISDIR(info.st_mode)) return "expected a directory";
  return final ? leaf_problem(info) : ancestor_problem(info);
}

const char* private_cache_problem(const fs::path& cache, fs::path& failed) {
  fs::path current;
  for (const auto& component : cache) {
    current /= component;
    if (const char* reason = component_problem(current, current == cache)) {
      failed = current;
      return reason;
    }
  }
  return nullptr;
}
#endif

fs::path resolve_cache(const fs::path& requested, std::string& error) {
  if (!requested.has_relative_path()) {
    error = "cannot use private Android build cache " + requested.string() + ": expected a user directory";
    return {};
  }
  std::error_code problem;
  const auto parent = fs::weakly_canonical(requested.parent_path(), problem);
  if (!problem) return parent / requested.filename();
  error = "cannot resolve Android build cache parent " + requested.parent_path().string() + ": " + problem.message();
  return {};
}

}  // namespace

fs::path cache_root_on(platform::Desktop desktop, const fs::path& home, const fs::path& named) {
  if (desktop == platform::Desktop::Windows) return named.is_absolute() ? named / "RedGem" / kCacheFolder : fs::path{};
  if (desktop == platform::Desktop::Mac) {
    if (!home.is_absolute()) return {};
    return home / "Library" / "Caches" / "RedGem" / kCacheFolder;
  }
  if (named.is_absolute()) return named / "redgem" / kCacheFolder;
  if (!home.is_absolute()) return {};
  return home / ".cache" / "redgem" / kCacheFolder;
}

fs::path cache_root() {
#ifdef _WIN32
  return cache_root_on(platform::this_desktop(), {}, local_app_data());
#else
  return cache_root_on(platform::this_desktop(), platform::environment_path("HOME"),
                       platform::environment_path("XDG_CACHE_HOME"));
#endif
}

fs::path ensure_cache(const fs::path& requested, std::string& error) {
  if (!requested.is_absolute()) {
    error = "cannot find a user cache for Android builds";
    return {};
  }
  const auto cache = resolve_cache(requested, error);
  if (cache.empty()) return {};
  fs::path failed;
  const char* reason = private_cache_problem(cache, failed);
  if (reason == nullptr) return cache;
  error = "cannot use private Android build cache " + failed.string() + ": " + reason;
  return {};
}

}  // namespace redgem::android_stage
