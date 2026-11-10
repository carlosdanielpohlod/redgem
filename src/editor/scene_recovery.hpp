#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <set>
#include <utility>

#include "editor/document/scene_document.hpp"
#include "editor/recovery_lease.hpp"
#include "storage/durable_file.hpp"

namespace redgem {
struct SceneRecoveryCopy {
  GlbFile glb;
  std::vector<std::uint8_t> bytes;
};
// One hidden, durable recovery copy beside the original; it never counts as a manual Save.
class SceneRecovery {
 public:
  explicit SceneRecovery(durable::Flush flush = durable::flush_to_disk) : flush_(std::move(flush)) {}
  static constexpr float kIntervalSeconds = 60.0f;
  static std::filesystem::path copy_path(const std::filesystem::path& source);
  void watch(const std::filesystem::path& source);
  void ignore();
  const std::string& problem() const { return problem_; }
  bool pending() const { return pending_; }
  bool source_changed() const;
  bool recover(SceneDocument& document, std::string& error);
  bool discard(std::string& error);
  bool poll(SceneDocument& document, float seconds, std::string& error);

 private:
  SceneRecoveryCopy* offered_copy(std::string& error) const;
  void discover_copy();
  bool retry_lease();
  bool interval_due(float seconds);
  bool own(const std::filesystem::path& source);
  void reset_observation();
  bool save_or_report(SceneDocument& document, std::string& error);
  bool restore(SceneDocument& document, std::string& error);
  bool remove_copy(std::string& error);
  bool write(SceneDocument& document, std::string& error);
  durable::Flush flush_;
  RecoveryLease lease_;
  std::string problem_;
  std::optional<long long> failed_version_;
  std::set<std::filesystem::path> ignored_sources_;
  bool save_version(SceneDocument& document, std::string& error);
  std::filesystem::path source_;
  std::string baseline_;
  std::string observed_stamp_;
  std::optional<long long> written_version_;
  float elapsed_ = 0;
  bool pending_ = false;
  mutable std::optional<bool> source_changed_;
  mutable std::optional<SceneRecoveryCopy> offered_;
};
}  // namespace redgem
