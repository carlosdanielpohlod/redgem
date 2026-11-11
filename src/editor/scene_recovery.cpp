#include "editor/scene_recovery.hpp"

#include <mbedtls/sha256.h>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string_view>

#include "storage/durable_file.hpp"
#include "core/platform.hpp"
#include "core/log.hpp"
#include "scene/scene_ids.hpp"
#include "editor/document/recovery_format.hpp"

namespace redgem {
namespace {
namespace fs = std::filesystem;
constexpr const char* kSuffix = ".recovery";
constexpr const char* kArchiveSuffix = ".previous-";
constexpr const char* kAppliedSuffix = ".applied-";
constexpr const char* kDismissalNotice = "Recovery dismissal applies only to this session: ";
using recovery_format::kMetadata;
constexpr const char* kSource = "source";
constexpr const char* kBaseline = "baseline";
constexpr const char* kVersion = "version";
constexpr int kFormatVersion = 1;
constexpr const char* kUnknownBaseline = "unknown";
constexpr std::uintmax_t kEagerFingerprintBytes = 1024 * 1024;
constexpr std::size_t kChunkBytes = 64 * 1024;
constexpr std::size_t kDigestBytes = 32;
constexpr char kHex[] = "0123456789abcdef";
constexpr std::size_t kHexDigitsPerByte = 2;
constexpr unsigned kNibbleBits = 4;
constexpr unsigned kNibbleMask = 15;
void require(bool condition, const char* reason) {
  if (!condition) throw std::runtime_error(reason);
}
struct Digest {
  mbedtls_sha256_context state{};
  Digest() { mbedtls_sha256_init(&state); }
  ~Digest() { mbedtls_sha256_free(&state); }
  Digest(const Digest&) = delete;
  Digest& operator=(const Digest&) = delete;
};
std::string hex(const std::array<unsigned char, kDigestBytes>& bytes) {
  std::string result;
  for (const auto byte : bytes) {
    result += kHex[byte >> kNibbleBits];
    result += kHex[byte & kNibbleMask];
  }
  return result;
}
bool feed_digest(std::ifstream& in, Digest& digest, std::uintmax_t budget) {
  std::array<unsigned char, kChunkBytes> chunk{};
  while (in) {
    in.read(reinterpret_cast<char*>(chunk.data()), chunk.size());
    const auto count = static_cast<std::uintmax_t>(in.gcount());
    if (count > budget) return false;
    budget -= count;
    require(mbedtls_sha256_update(&digest.state, chunk.data(), static_cast<std::size_t>(count)) == 0,
            "cannot fingerprint scene");
  }
  return in.eof();
}
std::string digest_contents(const fs::path& file, std::uintmax_t budget) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return {};
  Digest digest;
  require(mbedtls_sha256_starts(&digest.state, 0) == 0, "cannot start scene fingerprint");
  if (!feed_digest(in, digest, budget)) return kUnknownBaseline;
  std::array<unsigned char, kDigestBytes> bytes{};
  require(mbedtls_sha256_finish(&digest.state, bytes.data()) == 0, "cannot finish scene fingerprint");
  return hex(bytes);
}
std::string fingerprint(const fs::path& file, std::uintmax_t budget = kEagerFingerprintBytes) {
  std::error_code error;
  const auto size = fs::file_size(file, error);
  if (error) return {};
  if (size > budget) return kUnknownBaseline;
  return digest_contents(file, budget);
}
std::string path_digest(const fs::path& source) {
  const auto name = fs::weakly_canonical(fs::absolute(source)).generic_u8string();
  std::array<unsigned char, kDigestBytes> digest{};
  require(mbedtls_sha256(reinterpret_cast<const unsigned char*>(name.data()), name.size(), digest.data(), 0) == 0,
          "cannot identify scene recovery");
  return hex(digest);
}
fs::path dismissed_path(const fs::path& source) {
  return platform::user_data_dir() / "scene-recovery" / (path_digest(source) + ".dismissed");
}
void clear_dismissal(const fs::path& source) {
  try {
    std::error_code error;
    fs::remove(dismissed_path(source), error);
    if (error) log::warning("cannot clear scene recovery dismissal: " + error.message());
  } catch (const std::exception& error) {
    log::warning("cannot clear scene recovery dismissal: " + std::string(error.what()));
  }
}
void archive_copy(const fs::path& source) {
  const auto copy = SceneRecovery::copy_path(source);
  if (!fs::exists(copy)) return;
  auto archived = copy;
  archived += kArchiveSuffix + make_scene_id();
  fs::rename(copy, archived);
  log::info("preserved another editor's recovery copy at " + archived.string());
}
bool is_archive(const fs::directory_entry& entry, const std::string& prefix) {
  const auto name = entry.path().filename().generic_u8string();
  return name.rfind(prefix, 0) == 0 && entry.symlink_status().type() == fs::file_type::regular;
}
fs::path next_archive(const fs::path& copy) {
  const auto prefix = copy.filename().generic_u8string() + kArchiveSuffix;
  const auto folder = copy.has_parent_path() ? copy.parent_path() : fs::path(".");
  fs::path found;
  fs::file_time_type newest{};
  for (const auto& entry : fs::directory_iterator(folder)) {
    if (!is_archive(entry, prefix)) continue;
    const auto time = entry.last_write_time();
    const bool preferred = found.empty() || time > newest || (time == newest && entry.path() < found);
    if (!preferred) continue;
    found = entry.path();
    newest = time;
  }
  return found;
}
bool dismissed_copy(const fs::path& source) {
  std::ifstream marker(dismissed_path(source), std::ios::binary);
  std::array<char, kDigestBytes * kHexDigitsPerByte> digest{};
  if (!marker.read(digest.data(), digest.size())) return false;
  if (marker.peek() != std::char_traits<char>::eof()) return false;
  try {
    return std::string(digest.data(), digest.size()) ==
           fingerprint(SceneRecovery::copy_path(source), recovery_format::kMaxBytes);
  } catch (const std::exception&) {
    return false;
  }
}
void safe_copy(const fs::path& source, const fs::path& copy) {
  require(!fs::is_symlink(fs::symlink_status(source)), "scene recovery source is a symbolic link");
  require(!fs::is_symlink(fs::symlink_status(copy)), "recovery copy is a symbolic link");
}
std::vector<std::uint8_t> copy_bytes(const fs::path& copy) {
  const auto size = fs::file_size(copy);
  require(size <= recovery_format::kMaxBytes, recovery_format::limit_error().c_str());
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  std::ifstream in(copy, std::ios::binary);
  require(static_cast<bool>(in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))),
          "cannot read scene recovery");
  require(in.peek() == std::char_traits<char>::eof(), "scene recovery changed while reading");
  return bytes;
}
std::optional<SceneRecoveryCopy> checked_copy(const fs::path& source, const fs::path& copy, std::string& error) {
  safe_copy(source, copy);
  auto bytes = copy_bytes(copy);
  auto glb = decode_glb(bytes, copy.string(), error);
  if (!glb) return std::nullopt;
  const auto& meta = glb->json.at(kMetadata);
  require(meta.at(kVersion) == kFormatVersion, "unknown recovery format");
  require(meta.at(kSource) == source.filename().generic_u8string(), "recovery belongs to another scene");
  require(meta.at(kBaseline).is_string(), "invalid recovery source fingerprint");
  return SceneRecoveryCopy{std::move(*glb), std::move(bytes)};
}
bool source_differs(const SceneRecoveryCopy* copy, const fs::path& source) {
  if (!copy) return true;
  const auto baseline = copy->glb.json.at(kMetadata).at(kBaseline).get<std::string>();
  const auto current = fingerprint(source);
  const bool unknown = baseline == kUnknownBaseline || current == kUnknownBaseline;
  return unknown || baseline != current;
}
nlohmann::json recovery_metadata(const fs::path& source, const std::string& baseline) {
  return {{kVersion, kFormatVersion},
          {kSource, source.filename().generic_u8string()},
          {kBaseline, baseline},
          {"generation", make_scene_id()}};
}
bool publish_copy(const GlbFile& glb, const fs::path& copy, const durable::Flush& flush, std::string& error) {
  const auto bytes = encode_glb(glb, error);
  if (!bytes) return false;
  require(bytes->size() <= recovery_format::kMaxBytes, recovery_format::limit_error().c_str());
  const std::string_view data(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  require(durable::replace_file(copy, data, flush), "cannot write and flush scene recovery");
  return true;
}
}  // namespace
fs::path SceneRecovery::copy_path(const fs::path& source) {
  return source.parent_path() / fs::u8path("." + source.filename().generic_u8string() + kSuffix);
}
void SceneRecovery::reset_observation() {
  source_changed_.reset();
  offered_.reset();
  written_version_.reset();
  failed_version_.reset();
  const bool dismissal_failed = problem_.rfind(kDismissalNotice, 0) == 0;
  if (!dismissal_failed) problem_.clear();
  elapsed_ = 0;
  offered_path_.clear();
  adopted_archive_.clear();
  preserve_live_ = false;
  inspection_pending_ = true;
}
bool SceneRecovery::own(const fs::path& source) {
  const bool already_owned = source_ == source && lease_.held();
  if (already_owned) return true;
  if (source_ != source) takeover_pending_ = false;
  source_ = source;
  auto lock = copy_path(source);
  lock += ".lock";
  const auto result = lease_.acquire(lock);
  if (result == RecoveryLease::Result::Busy) takeover_pending_ = true;
  return result == RecoveryLease::Result::Acquired;
}
void SceneRecovery::prepare_copy(bool offer_archives, bool allow_live_offer) {
  const auto copy = copy_path(source_);
  const bool exists = fs::exists(fs::symlink_status(copy));
  preserve_live_ = ignored_sources_.count(source_) != 0 || (exists && dismissed_copy(source_));
  const bool has_live = exists && !preserve_live_;
  const bool archive_live = takeover_pending_ || !allow_live_offer;
  if (has_live && archive_live) archive_copy(source_);
  if (has_live && !archive_live) offered_path_ = copy;
  if (!pending() && offer_archives) offered_path_ = next_archive(copy);
  takeover_pending_ = false;
  inspection_pending_ = false;
}
void SceneRecovery::begin_watch(const fs::path& source, bool offer_archives) {
  reset_observation();
  baseline_ = kUnknownBaseline;
  if (!own(source)) {
    problem_ = "Scene recovery unavailable: another editor owns it, or the folder is not writable";
    return;
  }
  const auto digest = fingerprint(source);
  if (!digest.empty()) baseline_ = digest;
  prepare_copy(offer_archives, true);
}
void SceneRecovery::watch(const fs::path& source, bool offer_archives) {
  try {
    begin_watch(source, offer_archives);
  } catch (const std::exception& error) {
    if (takeover_pending_) lease_.release();
    problem_ = "Scene recovery unavailable: " + std::string(error.what());
  }
}
bool SceneRecovery::retry_lease(bool clean) {
  const bool ready = lease_.held() && !inspection_pending_;
  if (ready) return true;
  if (!own(source_)) return false;
  prepare_copy(false, clean);
  problem_.clear();
  return true;
}
void SceneRecovery::ignore() {
  const auto ignored = std::move(offered_path_);
  offered_path_.clear();
  offered_.reset();
  const bool archive = !ignored.empty() && ignored != copy_path(source_);
  if (archive) return;
  ignored_sources_.insert(source_);
  preserve_live_ = true;
  const bool foreign_copy = !lease_.held() || takeover_pending_;
  if (!foreign_copy) persist_dismissal();
}
void SceneRecovery::persist_dismissal() {
  try {
    const auto digest = fingerprint(copy_path(source_), recovery_format::kMaxBytes);
    require(digest.size() == kDigestBytes * kHexDigitsPerByte, "cannot fingerprint the ignored recovery copy");
    fs::create_directories(dismissed_path(source_).parent_path());
    require(durable::replace_file(dismissed_path(source_), digest, flush_), "cannot persist recovery dismissal");
  } catch (const std::exception& error) {
    problem_ = std::string(kDismissalNotice) + error.what();
  }
}
SceneRecoveryCopy* SceneRecovery::offered_copy(std::string& error) const {
  if (!pending()) return nullptr;
  if (!offered_) offered_ = checked_copy(source_, offered_path_, error);
  return offered_ ? &*offered_ : nullptr;
}
bool SceneRecovery::source_changed() const {
  if (source_changed_) return *source_changed_;
  try {
    std::string error;
    const auto* copy = offered_copy(error);
    source_changed_ = source_differs(copy, source_);
  } catch (const std::exception&) {
    source_changed_ = true;
  }
  return *source_changed_;
}
bool SceneRecovery::restore(SceneDocument& document, std::string& error) {
  require(lease_.held(), "scene recovery is owned by another editor");
  auto* checked = offered_copy(error);
  if (!checked) return false;
  const auto recovered_baseline = checked->glb.json.at(kMetadata).at(kBaseline).get<std::string>();
  auto opened = SceneDocument::open_recovery(source_, std::move(checked->glb), checked->bytes);
  offered_.reset();
  if (auto* problem = std::get_if<std::string>(&opened)) {
    error = *problem;
    return false;
  }
  document = std::move(std::get<SceneDocument>(opened));
  baseline_ = recovered_baseline;
  if (offered_path_ != copy_path(source_)) adopted_archive_ = std::move(offered_path_);
  offered_path_.clear();
  return true;
}
bool SceneRecovery::recover(SceneDocument& document, std::string& error) {
  try {
    return restore(document, error);
  } catch (const std::exception& problem) {
    error = problem.what();
    return false;
  }
}
void SceneRecovery::retire_archive() {
  if (adopted_archive_.empty()) return;
  try {
    auto applied = copy_path(source_);
    applied += kAppliedSuffix + make_scene_id();
    fs::rename(adopted_archive_, applied);
    adopted_archive_.clear();
  } catch (const std::exception& error) {
    log::warning("cannot mark scene recovery as applied: " + std::string(error.what()));
  }
}
bool SceneRecovery::remove_copy(std::string& error) {
  const bool unsafe = !lease_.held() || takeover_pending_ || inspection_pending_;
  if (unsafe) return true;
  std::error_code problem;
  fs::remove(copy_path(source_), problem);
  if (problem) {
    error = "cannot remove scene recovery: " + problem.message();
    return false;
  }
  clear_dismissal(source_);
  offered_path_.clear();
  offered_.reset();
  written_version_.reset();
  return true;
}
bool SceneRecovery::discard(std::string& error) {
  try {
    const bool archive = pending() && offered_path_ != copy_path(source_);
    if (!archive) return remove_copy(error);
    require(lease_.held(), "scene recovery is owned by another editor");
    fs::remove(offered_path_);
    offered_path_.clear();
    offered_.reset();
    source_changed_.reset();
    return true;
  } catch (const std::exception& problem) {
    error = problem.what();
    return false;
  }
}
bool SceneRecovery::discard_unsaved(std::string& error) { return pending() || preserve_live_ || discard(error); }
bool SceneRecovery::write(SceneDocument& document, std::string& error) {
  const auto copy = copy_path(source_);
  safe_copy(source_, copy);
  auto glb = document.recovery_copy(error);
  if (!glb) return false;
  glb->json[kMetadata] = recovery_metadata(source_, baseline_);
  if (!publish_copy(*glb, copy, flush_, error)) return false;
  clear_dismissal(source_);
  return true;
}
bool SceneRecovery::save_version(SceneDocument& document, std::string& error) {
  try {
    if (!write(document, error)) return false;
    written_version_ = document.version();
    ignored_sources_.erase(source_);
    preserve_live_ = false;
    retire_archive();
    return true;
  } catch (const std::exception& problem) {
    error = problem.what();
    return false;
  }
}
bool SceneRecovery::save_or_report(SceneDocument& document, std::string& error) {
  const bool saved = document.dirty() ? save_version(document, error) : discard_unsaved(error);
  if (saved) {
    problem_.clear();
    return true;
  }
  failed_version_ = document.version();
  problem_ = "Scene recovery unavailable: " + error;
  return false;
}
bool SceneRecovery::interval_due(float seconds) {
  elapsed_ += seconds;
  if (elapsed_ < kIntervalSeconds) return false;
  elapsed_ = 0;
  return true;
}
bool SceneRecovery::poll_ready(SceneDocument& document, float seconds, std::string& error) {
  const bool blocked = pending() || failed_version_ == document.version();
  if (blocked) return true;
  if (!interval_due(seconds)) return true;
  const bool prepared = retry_lease(!document.dirty()) && !pending();
  if (!prepared) return true;
  const bool already_written = document.dirty() && written_version_ == document.version();
  if (already_written) return true;
  return save_or_report(document, error);
}
bool SceneRecovery::poll(SceneDocument& document, float seconds, std::string& error) {
  try {
    return poll_ready(document, seconds, error);
  } catch (const std::exception& problem) {
    error = problem.what();
    problem_ = "Scene recovery unavailable: " + error;
    if (takeover_pending_) lease_.release();
    return false;
  }
}
}  // namespace redgem
