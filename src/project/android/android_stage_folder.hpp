#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "core/files.hpp"
#include "editor/recovery_lease.hpp"

namespace redgem::android_stage {

inline constexpr const char* kPrefix = ".redgem-apk-stage-";
inline constexpr const char* kOwnerFile = ".redgem-stage-owner";
inline constexpr char kOwnerSignature[] = "redgem-android-stage-v1\n";
inline constexpr const char* kLeaseFile = ".redgem-stage-lease";
inline constexpr const char* kPackageFolder = "package";

// Owns one build's staging folder until packing finishes.
class Folder {
 public:
  Folder(const std::filesystem::path& output, std::string& error);
  ~Folder();
  Folder(const Folder&) = delete;
  Folder& operator=(const Folder&) = delete;

  std::filesystem::path path() const;

 private:
  bool mark_owned(std::string& error);
  std::unique_ptr<ReservedFolder> folder_;
  RecoveryLease lease_;
};

}  // namespace redgem::android_stage
