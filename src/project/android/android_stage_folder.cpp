#include "project/android/android_stage_folder.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <string_view>
#include <thread>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "core/log.hpp"
#include "core/platform.hpp"

namespace redgem::android_stage {
namespace {
namespace fs = std::filesystem;

constexpr const char* kMaintenanceFile = ".redgem-apk-stage.lock";
constexpr int kMaintenanceAttempts = 50;
constexpr std::chrono::milliseconds kMaintenanceDelay{20};
constexpr std::chrono::minutes kUnmarkedStageAge{10};

bool hold_maintenance(RecoveryLease& lease, const fs::path& cache) {
  const auto lock = cache / kMaintenanceFile;
  for (int attempt = 0; attempt < kMaintenanceAttempts; ++attempt) {
    const auto result = lease.acquire(lock);
    if (result != RecoveryLease::Result::Busy) return result == RecoveryLease::Result::Acquired;
    std::this_thread::sleep_for(kMaintenanceDelay);
  }
  return false;
}

bool signature_matches(const fs::path& marker) {
  std::ifstream in(marker, std::ios::binary);
  std::array<char, sizeof(kOwnerSignature) - 1> content{};
  if (!in.read(content.data(), static_cast<std::streamsize>(content.size()))) return false;
  return std::string_view(content.data(), content.size()) == kOwnerSignature &&
         in.peek() == std::char_traits<char>::eof();
}

bool generated_suffix(std::string_view suffix) {
  const auto digits = [](std::string_view part) {
    return !part.empty() && std::all_of(part.begin(), part.end(), [](char c) { return c >= '0' && c <= '9'; });
  };
  const auto dash = suffix.find('-');
  if (dash == std::string_view::npos) return digits(suffix);
  return digits(suffix.substr(0, dash)) && digits(suffix.substr(dash + 1));
}

bool only_reservation_files(const fs::path& stage) {
  for (const auto& entry : fs::directory_iterator(stage)) {
    if (entry.path().filename() != kLeaseFile) return false;
  }
  return true;
}

bool is_old_reservation(const fs::path& stage, std::string_view suffix) {
  if (!generated_suffix(suffix)) return false;
  std::error_code problem;
  const auto time = fs::last_write_time(stage, problem);
  const bool old = !problem && time <= fs::file_time_type::clock::now() - kUnmarkedStageAge;
  return old && only_reservation_files(stage);
}

bool is_owned_stage(const fs::path& stage) {
  const auto name = stage.filename().generic_u8string();
  std::error_code problem;
  const bool directory = fs::is_directory(fs::symlink_status(stage, problem));
  if (!directory || name.rfind(kPrefix, 0) != 0) return false;
  const auto marker = stage / kOwnerFile;
  const auto status = fs::symlink_status(marker, problem);
  if (fs::is_regular_file(status)) return signature_matches(marker);
  if (status.type() != fs::file_type::not_found) return false;
  return is_old_reservation(stage, std::string_view(name).substr(std::string_view(kPrefix).size()));
}

bool remove_payload(const fs::path& stage) {
  std::error_code problem;
  fs::remove_all(stage / kPackageFolder, problem);
  if (!problem) return true;
  log::warning("cannot remove Android staging payload in " + stage.string() + ": " + problem.message());
  return false;
}

// The stage lease stays held during the potentially slow payload deletion. Only the short
// namespace operation needs the cache lock, which also prevents reuse before deletion finishes.
void reclaim(const fs::path& stage, RecoveryLease& lease) {
  if (!remove_payload(stage)) return;
  RecoveryLease maintenance;
  if (!hold_maintenance(maintenance, stage.parent_path())) {
    log::warning("Android staging cleanup deferred for " + stage.string());
    return;
  }
  lease.release();
  std::error_code problem;
  fs::remove_all(stage, problem);
  if (problem) log::warning("cannot remove Android staging folder " + stage.string() + ": " + problem.message());
}

bool hold_stale(RecoveryLease& lease, const fs::path& stage) {
  RecoveryLease maintenance;
  if (!hold_maintenance(maintenance, stage.parent_path())) return false;
  if (!is_owned_stage(stage)) return false;
  return lease.acquire(stage / kLeaseFile) == RecoveryLease::Result::Acquired;
}

void sweep(const fs::path& cache) {
  try {
    for (const auto& entry : fs::directory_iterator(cache)) {
      if (entry.path().filename().generic_u8string().rfind(kPrefix, 0) != 0) continue;
      RecoveryLease lease;
      if (hold_stale(lease, entry.path())) reclaim(entry.path(), lease);
    }
  } catch (const fs::filesystem_error& error) {
    log::warning("cannot inspect Android build folders: " + std::string(error.what()));
  }
}

bool private_cache(const fs::path& cache) {
#ifdef _WIN32
  std::error_code problem;
  fs::create_directory(cache, problem);
  return fs::is_directory(fs::symlink_status(cache, problem));
#else
  constexpr mode_t kOwnerOnly = S_IRWXU;
  if (::mkdir(cache.c_str(), kOwnerOnly) == 0) return true;
  struct stat status{};
  if (::lstat(cache.c_str(), &status) != 0) return false;
  const bool owned = S_ISDIR(status.st_mode) && status.st_uid == ::geteuid();
  return owned && (status.st_mode & (S_IRWXG | S_IRWXO)) == 0;
#endif
}

bool ensure_cache(const fs::path& cache, std::string& error) {
  std::error_code problem;
  fs::create_directories(cache.parent_path(), problem);
  if (!problem && private_cache(cache)) return true;
  error = "cannot use private Android build cache " + cache.string();
  if (problem) error += ": " + problem.message();
  return false;
}
}  // namespace

fs::path cache_root() {
  const auto home = platform::home_dir();
  if (platform::is_mac()) return home / "Library" / "Caches" / "RedGem" / "android-stage";
  const auto named = platform::environment_path("XDG_CACHE_HOME");
  const auto cache = named.is_absolute() ? named : home / ".cache";
  return cache / "redgem" / "android-stage";
}

Folder::Folder(const fs::path& cache, std::string& error) {
  if (!ensure_cache(cache, error)) return;
  sweep(cache);
  RecoveryLease maintenance;
  if (!hold_maintenance(maintenance, cache)) {
    error = "cannot lock Android build cache " + cache.string();
    return;
  }
  folder_ = std::make_unique<ReservedFolder>(cache, kPrefix, error);
  if (folder_->path().empty() || !mark_owned(error)) folder_.reset();
}

bool Folder::mark_owned(std::string& error) {
  const auto& stage = folder_->path();
  if (lease_.acquire(stage / kLeaseFile) != RecoveryLease::Result::Acquired) {
    error = "cannot lock Android staging folder " + stage.string();
    return false;
  }
  if (write_text_file(stage / kOwnerFile, kOwnerSignature, error)) return true;
  lease_.release();
  return false;
}

Folder::~Folder() {
  if (!folder_) return;
  const auto stage = folder_->path();
  folder_->keep();
  reclaim(stage, lease_);
}

fs::path Folder::path() const { return folder_ ? folder_->path() : fs::path{}; }

}  // namespace redgem::android_stage
