#include "editor/scene_recovery.hpp"

#include <mbedtls/sha256.h>
#include <array>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string_view>

#include "storage/durable_file.hpp"
#include "core/platform.hpp"
#include "scene/scene_ids.hpp"
#include "editor/document/recovery_format.hpp"

namespace redgem {
namespace {
namespace fs = std::filesystem;
constexpr const char* kSuffix = ".recovery";
constexpr const char* kDismissalNotice = "Recovery dismissal applies only to this session: ";
using recovery_format::kMetadata;
constexpr const char* kSource = "source";
constexpr const char* kBaseline = "baseline";
constexpr const char* kVersion = "version";
constexpr int kFormatVersion = 1;
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
bool feed_digest(std::ifstream& in, Digest& digest) {
  std::array<unsigned char, kChunkBytes> chunk{};
  while (in) {
    in.read(reinterpret_cast<char*>(chunk.data()), chunk.size());
    require(mbedtls_sha256_update(&digest.state, chunk.data(), static_cast<std::size_t>(in.gcount())) == 0,
            "cannot fingerprint scene");
  }
  return in.eof();
}
std::string fingerprint(const fs::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return {};
  Digest digest;
  require(mbedtls_sha256_starts(&digest.state, 0) == 0, "cannot start scene fingerprint");
  if (!feed_digest(in, digest)) return {};
  std::array<unsigned char, kDigestBytes> bytes{};
  require(mbedtls_sha256_finish(&digest.state, bytes.data()) == 0, "cannot finish scene fingerprint");
  return hex(bytes);
}
std::string file_stamp(const fs::path& file) {
  std::error_code error;
  const auto size = fs::file_size(file, error);
  if (error) return "missing";
  const auto time = fs::last_write_time(file, error);
  if (error) return "unknown";
  const auto ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
  return std::to_string(size) + ":" + std::to_string(ticks);
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
  std::error_code error;
  fs::remove(dismissed_path(source), error);
  require(!error, "cannot clear scene recovery dismissal");
}
bool dismissed_copy(const fs::path& source) {
  std::ifstream marker(dismissed_path(source), std::ios::binary);
  std::array<char, kDigestBytes * kHexDigitsPerByte> digest{};
  if (!marker.read(digest.data(), digest.size())) return false;
  if (marker.peek() != std::char_traits<char>::eof()) return false;
  try {
    return std::string(digest.data(), digest.size()) == fingerprint(SceneRecovery::copy_path(source));
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
std::optional<SceneRecoveryCopy> checked_copy(const fs::path& source, std::string& error) {
  const auto copy = SceneRecovery::copy_path(source);
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
  pending_ = false;
}
bool SceneRecovery::own(const fs::path& source) {
  const bool already_owned = source_ == source && lease_.held();
  source_ = source;
  auto lock = copy_path(source);
  lock += ".lock";
  return already_owned || lease_.acquire(lock);
}
void SceneRecovery::discover_copy() {
  std::error_code error;
  const bool ignored = ignored_sources_.count(source_) != 0;
  const bool exists = fs::exists(fs::symlink_status(copy_path(source_), error));
  pending_ = !ignored && exists && !dismissed_copy(source_);
}
void SceneRecovery::watch(const fs::path& source) {
  reset_observation();
  baseline_.clear();
  observed_stamp_ = file_stamp(source);
  std::error_code error;
  const bool small = fs::file_size(source, error) <= kEagerFingerprintBytes && !error;
  if (small) baseline_ = fingerprint(source);
  if (!own(source)) {
    problem_ = "Scene recovery unavailable: another editor owns it, or the folder is not writable";
    return;
  }
  discover_copy();
}
bool SceneRecovery::retry_lease() {
  if (lease_.held()) return true;
  if (!own(source_)) return false;
  problem_.clear();
  discover_copy();
  return true;
}
void SceneRecovery::ignore() {
  ignored_sources_.insert(source_);
  pending_ = false;
  offered_.reset();
  if (!lease_.held()) return;
  try {
    const auto digest = fingerprint(copy_path(source_));
    require(!digest.empty(), "cannot fingerprint the ignored recovery copy");
    fs::create_directories(dismissed_path(source_).parent_path());
    require(durable::replace_file(dismissed_path(source_), digest, flush_), "cannot persist recovery dismissal");
  } catch (const std::exception& error) {
    problem_ = std::string(kDismissalNotice) + error.what();
  }
}
SceneRecoveryCopy* SceneRecovery::offered_copy(std::string& error) const {
  if (!offered_) offered_ = checked_copy(source_, error);
  return offered_ ? &*offered_ : nullptr;
}
bool SceneRecovery::source_changed() const {
  if (source_changed_) return *source_changed_;
  try {
    std::string error;
    const auto* copy = offered_copy(error);
    const bool changed = !copy || copy->glb.json.at(kMetadata).at(kBaseline) != fingerprint(source_);
    source_changed_ = changed;
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
  pending_ = false;
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
bool SceneRecovery::remove_copy(std::string& error) {
  if (!lease_.held()) return true;
  std::error_code problem;
  fs::remove(copy_path(source_), problem);
  if (problem) {
    error = "cannot remove scene recovery: " + problem.message();
    return false;
  }
  clear_dismissal(source_);
  pending_ = false;
  offered_.reset();
  written_version_.reset();
  return true;
}
bool SceneRecovery::discard(std::string& error) {
  try {
    return remove_copy(error);
  } catch (const std::exception& problem) {
    error = problem.what();
    return false;
  }
}
bool SceneRecovery::write(SceneDocument& document, std::string& error) {
  const auto copy = copy_path(source_);
  safe_copy(source_, copy);
  if (baseline_.empty()) {
    const bool unchanged = file_stamp(source_) == observed_stamp_;
    baseline_ = unchanged ? fingerprint(source_) : "changed:" + observed_stamp_;
  }
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
    return true;
  } catch (const std::exception& problem) {
    error = problem.what();
    return false;
  }
}
bool SceneRecovery::save_or_report(SceneDocument& document, std::string& error) {
  const bool saved = document.dirty() ? save_version(document, error) : discard(error);
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
bool SceneRecovery::poll(SceneDocument& document, float seconds, std::string& error) {
  if (pending_) return true;
  if (!interval_due(seconds)) return true;
  if (!retry_lease()) return true;
  const bool blocked = pending_ || failed_version_ == document.version();
  if (blocked) return true;
  if (!document.dirty()) return save_or_report(document, error);
  if (written_version_ == document.version()) return true;
  return save_or_report(document, error);
}
}  // namespace redgem
