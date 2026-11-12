#include "project/android/android_stage_folder.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "core/log.hpp"
#include "project/android/android_stage_cache.hpp"

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

bool reservation_file(const fs::directory_entry& entry, std::error_code& problem) {
  const auto name = entry.path().filename();
  const bool metadata = name == kLeaseFile || name == kPendingOwnerFile;
  return metadata && fs::is_regular_file(entry.symlink_status(problem));
}

bool only_reservation_files(const fs::path& stage) {
  std::error_code problem;
  const fs::directory_iterator end;
  for (fs::directory_iterator it(stage, problem); !problem && it != end; it.increment(problem)) {
    if (!reservation_file(*it, problem)) return false;
  }
  return !problem;
}

bool is_old_reservation(const fs::path& stage, std::string_view suffix) {
  if (!generated_suffix(suffix)) return false;
  std::error_code problem;
  const auto time = fs::last_write_time(stage, problem);
  const bool old = !problem && time <= fs::file_time_type::clock::now() - kUnmarkedStageAge;
  return old && only_reservation_files(stage);
}

bool stage_directory(const fs::path& stage, const std::string& name) {
  if (name.rfind(kPrefix, 0) != 0) return false;
  std::error_code problem;
  return fs::is_directory(fs::symlink_status(stage, problem));
}

enum class StageKind { Other, Incomplete, Marked };

StageKind stage_kind(const fs::path& stage) {
  const auto name = stage.filename().generic_u8string();
  if (!stage_directory(stage, name)) return StageKind::Other;
  std::error_code problem;
  const auto marker = stage / kOwnerFile;
  const auto status = fs::symlink_status(marker, problem);
  if (fs::is_regular_file(status) && signature_matches(marker)) return StageKind::Marked;
  if (status.type() != fs::file_type::not_found) return StageKind::Other;
  const auto suffix = std::string_view(name).substr(std::string_view(kPrefix).size());
  return is_old_reservation(stage, suffix) ? StageKind::Incomplete : StageKind::Other;
}

bool remove_payload(const fs::path& stage) {
  std::error_code problem;
  fs::remove_all(stage / kPackageFolder, problem);
  if (!problem) return true;
  log::warning("cannot remove Android staging payload in " + stage.string() + ": " + problem.message());
  return false;
}

// The cache lock must be held while closing the stage lease and removing its directory.
void finish_stage(const fs::path& stage) {
  std::error_code problem;
  for (const char* file : {kPendingOwnerFile, kLeaseFile, kOwnerFile}) {
    fs::remove(stage / file, problem);
    if (problem) break;
  }
  if (!problem) fs::remove(stage, problem);
  if (problem) log::warning("cannot remove Android staging folder " + stage.string() + ": " + problem.message());
}

void finish_incomplete(const fs::path& stage) {
  const auto lock = stage / kLeaseFile;
  std::error_code problem;
  if (fs::symlink_status(lock, problem).type() == fs::file_type::not_found) {
    finish_stage(stage);
    return;
  }
  RecoveryLease lease;
  if (lease.acquire(lock) != RecoveryLease::Result::Acquired) return;
  lease.release();
  finish_stage(stage);
}

struct StaleStage {
  fs::path path;
  std::unique_ptr<RecoveryLease> lease;
};

void inspect_stage(std::vector<StaleStage>& stale, const fs::path& stage) {
  switch (stage_kind(stage)) {
    case StageKind::Other:
      return;
    case StageKind::Incomplete:
      finish_incomplete(stage);
      return;
    case StageKind::Marked:
      break;
  }
  auto lease = std::make_unique<RecoveryLease>();
  if (lease->acquire(stage / kLeaseFile) == RecoveryLease::Result::Acquired) {
    stale.push_back({stage, std::move(lease)});
  }
}

std::vector<StaleStage> collect_stale(const fs::path& cache) {
  RecoveryLease maintenance;
  if (!hold_maintenance(maintenance, cache)) return {};
  std::vector<StaleStage> stale;
  std::error_code problem;
  const fs::directory_iterator end;
  for (fs::directory_iterator it(cache, problem); !problem && it != end; it.increment(problem)) {
    inspect_stage(stale, it->path());
  }
  if (problem) log::warning("cannot inspect Android build folders: " + problem.message());
  return stale;
}

// Only this batch's namespace changes run under the cache lock. Payload deletion holds
// each stage's lease and happens outside that lock, regardless of the payload size.
void finish_batch(const fs::path& cache, std::vector<StaleStage>& stale) {
  if (stale.empty()) return;
  RecoveryLease maintenance;
  if (!hold_maintenance(maintenance, cache)) {
    log::warning("Android staging cleanup deferred in " + cache.string());
    return;
  }
  for (auto& stage : stale) {
    stage.lease->release();
    finish_stage(stage.path);
  }
}

void sweep(const fs::path& cache) {
  auto stale = collect_stale(cache);
  const auto kept =
      std::remove_if(stale.begin(), stale.end(), [](const StaleStage& stage) { return !remove_payload(stage.path); });
  stale.erase(kept, stale.end());
  finish_batch(cache, stale);
}

bool publish_marker(const fs::path& stage, std::string& error) {
  const auto temp = stage / kPendingOwnerFile;
  std::ofstream out(temp, std::ios::binary);
  out << kOwnerSignature;
  out.close();
  if (!out) {
    error = "cannot write Android staging marker " + temp.string();
    return false;
  }
  std::error_code problem;
  fs::rename(temp, stage / kOwnerFile, problem);
  if (!problem) return true;
  error = "cannot publish Android staging marker " + temp.string() + ": " + problem.message();
  return false;
}
}  // namespace

Folder::Folder(const fs::path& requested, std::string& error) {
  const auto cache = normalized_folder(requested);
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
  if (publish_marker(stage, error)) return true;
  lease_.release();
  return false;
}

Folder::~Folder() {
  if (!folder_) return;
  const auto stage = folder_->path();
  folder_->keep();
  if (!remove_payload(stage)) return;
  RecoveryLease maintenance;
  if (!hold_maintenance(maintenance, stage.parent_path())) {
    log::warning("Android staging cleanup deferred for " + stage.string());
    return;
  }
  lease_.release();
  finish_stage(stage);
}

fs::path Folder::path() const { return folder_ ? folder_->path() : fs::path{}; }

}  // namespace redgem::android_stage
