#include "project/android/android_stage_folder.hpp"

#include <array>
#include <chrono>
#include <fstream>
#include <string_view>
#include <thread>

#include "core/log.hpp"

namespace redgem::android_stage {
namespace {
namespace fs = std::filesystem;

constexpr const char* kMaintenanceFile = ".redgem-apk-stage.lock";
constexpr int kMaintenanceAttempts = 50;
constexpr std::chrono::milliseconds kMaintenanceDelay{20};

bool hold_maintenance(RecoveryLease& lease, const fs::path& output) {
  const auto lock = output / kMaintenanceFile;
  for (int attempt = 0; attempt < kMaintenanceAttempts; ++attempt) {
    const auto result = lease.acquire(lock);
    if (result != RecoveryLease::Result::Busy) return result == RecoveryLease::Result::Acquired;
    std::this_thread::sleep_for(kMaintenanceDelay);
  }
  return false;
}

bool is_owned_stage(const fs::directory_entry& entry) {
  const auto name = entry.path().filename().generic_u8string();
  std::error_code problem;
  const bool directory = fs::is_directory(entry.symlink_status(problem));
  if (!directory || name.rfind(kPrefix, 0) != 0) return false;
  const auto marker = entry.path() / kOwnerFile;
  if (!fs::is_regular_file(fs::symlink_status(marker, problem))) return false;
  std::ifstream in(marker, std::ios::binary);
  std::array<char, sizeof(kOwnerSignature) - 1> content{};
  if (!in.read(content.data(), static_cast<std::streamsize>(content.size()))) return false;
  return std::string_view(content.data(), content.size()) == kOwnerSignature &&
         in.peek() == std::char_traits<char>::eof();
}

bool remove_payload(const fs::path& stage) {
  std::error_code problem;
  fs::remove_all(stage / kPackageFolder, problem);
  if (!problem) return true;
  log::warning("cannot remove Android staging payload in " + stage.string() + ": " + problem.message());
  return false;
}

void remove_stale(const fs::path& stage) {
  RecoveryLease lease;
  if (lease.acquire(stage / kLeaseFile) != RecoveryLease::Result::Acquired) return;
  // Every allocation and deletion holds the parent lock, so the name cannot be reused here.
  lease.release();
  if (!remove_payload(stage)) return;
  std::error_code problem;
  fs::remove_all(stage, problem);
  if (problem)
    log::warning("cannot remove interrupted Android build folder " + stage.string() + ": " + problem.message());
}

void sweep(const fs::path& output) {
  try {
    for (const auto& entry : fs::directory_iterator(output)) {
      if (is_owned_stage(entry)) remove_stale(entry.path());
    }
  } catch (const fs::filesystem_error& error) {
    log::warning("cannot inspect Android build folders: " + std::string(error.what()));
  }
}

bool ensure_output(const fs::path& output, std::string& error) {
  std::error_code problem;
  fs::create_directories(output, problem);
  if (!problem) return true;
  error = "cannot create Android output folder: " + problem.message();
  return false;
}
}  // namespace

Folder::Folder(const fs::path& output, std::string& error) {
  if (!ensure_output(output, error)) return;
  RecoveryLease maintenance;
  if (!hold_maintenance(maintenance, output)) {
    error = "cannot lock Android staging in " + output.string();
    return;
  }
  sweep(output);
  folder_ = std::make_unique<ReservedFolder>(output, kPrefix, error);
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
  RecoveryLease maintenance;
  if (!hold_maintenance(maintenance, folder_->path().parent_path())) {
    log::warning("Android staging cleanup deferred for " + folder_->path().string());
    folder_->keep();
    return;
  }
  lease_.release();
  if (!remove_payload(folder_->path())) folder_->keep();
  folder_.reset();
}

fs::path Folder::path() const { return folder_ ? folder_->path() : fs::path{}; }

}  // namespace redgem::android_stage
