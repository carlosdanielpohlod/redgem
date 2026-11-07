#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include "editor/document/scene_document.hpp"
#include "storage/durable_file.hpp"

namespace redgem {
// One hidden, durable recovery copy beside the original; it never counts as a manual Save.
class SceneRecovery {
 public:
  explicit SceneRecovery(durable::Flush flush = durable::flush_to_disk) : flush_(std::move(flush)) {}
  static constexpr float kIntervalSeconds = 60.0f;
  static std::filesystem::path copy_path(const std::filesystem::path& source);
  void watch(const std::filesystem::path& source);
  bool pending() const { return pending_; }
  bool source_changed() const;
  bool recover(SceneDocument& document, std::string& error);
  bool discard(std::string& error);
  bool poll(SceneDocument& document, float seconds, std::string& error);

 private:
  bool write(SceneDocument& document, std::string& error);
  durable::Flush flush_;
  bool save_version(SceneDocument& document, std::string& error);
  std::filesystem::path source_;
  std::string baseline_;
  std::optional<long long> written_version_;
  float elapsed_ = 0;
  bool pending_ = false;
  mutable std::optional<bool> source_changed_;
};
}  // namespace redgem
