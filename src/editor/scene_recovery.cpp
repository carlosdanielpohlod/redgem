#include "editor/scene_recovery.hpp"

#include <mbedtls/sha256.h>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string_view>

#include "storage/durable_file.hpp"
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
  std::error_code size_error;
  const auto size = fs::file_size(file, size_error);
  require(size_error || size <= recovery_format::kMaxBytes, recovery_format::limit_error().c_str());
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
  return std::to_string(size) + ":" + std::to_string(time.time_since_epoch().count());
}
fs::path dismissed_path(const fs::path& source) {
  auto result = SceneRecovery::copy_path(source);
  result += ".dismissed";
  return result;
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
std::optional<GlbFile> checked_copy(const fs::path& source, std::string& error) {
  const auto copy = SceneRecovery::copy_path(source);
  safe_copy(source, copy);
  require(fs::file_size(copy) <= recovery_format::kMaxBytes, recovery_format::limit_error().c_str());
  auto glb = read_glb(copy, error);
  if (!glb) return std::nullopt;
  const auto& meta = glb->json.at(kMetadata);
  require(meta.at(kVersion) == kFormatVersion, "unknown recovery format");
  require(meta.at(kSource) == source.filename().generic_u8string(), "recovery belongs to another scene");
  require(meta.at(kBaseline).is_string(), "invalid recovery source fingerprint");
  return glb;
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
void SceneRecovery::watch(const fs::path& source) {
  reset_observation();
  if (!own(source)) {
    problem_ = "Scene recovery unavailable: another editor owns it, or the folder is not writable";
    return;
  }
  baseline_.clear();
  observed_stamp_ = file_stamp(source);
  std::error_code error;
  const bool ignored = ignored_sources_.count(source) != 0;
  const bool exists = fs::exists(fs::symlink_status(copy_path(source), error));
  pending_ = !ignored && exists && !dismissed_copy(source);
}
void SceneRecovery::ignore() {
  ignored_sources_.insert(source_);
  pending_ = false;
  offered_.reset();
  if (!lease_.held()) return;
  try {
    const auto digest = fingerprint(copy_path(source_));
    require(!digest.empty(), "cannot fingerprint the ignored recovery copy");
    require(durable::replace_file(dismissed_path(source_), digest, flush_), "cannot persist recovery dismissal");
  } catch (const std::exception& error) {
    problem_ = std::string(kDismissalNotice) + error.what();
  }
}
const GlbFile* SceneRecovery::offered_copy(std::string& error) const {
  if (!offered_) offered_ = checked_copy(source_, error);
  return offered_ ? &*offered_ : nullptr;
}
bool SceneRecovery::source_changed() const {
  if (source_changed_) return *source_changed_;
  try {
    std::string error;
    const auto* copy = offered_copy(error);
    const bool changed = !copy || copy->json.at(kMetadata).at(kBaseline) != fingerprint(source_);
    source_changed_ = changed;
  } catch (const std::exception&) {
    source_changed_ = true;
  }
  return *source_changed_;
}
bool SceneRecovery::restore(SceneDocument& document, std::string& error) {
  require(lease_.held(), "scene recovery is owned by another editor");
  const auto* checked = offered_copy(error);
  if (!checked) return false;
  const auto recovered_baseline = checked->json.at(kMetadata).at(kBaseline).get<std::string>();
  auto opened = SceneDocument::open_recovery(source_, std::move(*offered_));
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
bool SceneRecovery::discard(std::string& error) {
  if (!lease_.held()) return true;
  std::error_code problem;
  fs::remove(copy_path(source_), problem);
  if (problem) {
    error = "cannot remove scene recovery: " + problem.message();
    return false;
  }
  pending_ = false;
  offered_.reset();
  written_version_.reset();
  return true;
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
  glb->json[kMetadata] = {
      {kVersion, kFormatVersion}, {kSource, source_.filename().generic_u8string()}, {kBaseline, baseline_}};
  return publish_copy(*glb, copy, flush_, error);
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
bool SceneRecovery::poll(SceneDocument& document, float seconds, std::string& error) {
  const bool blocked = pending_ || !lease_.held() || failed_version_ == document.version();
  if (blocked) return true;
  elapsed_ += seconds;
  if (elapsed_ < kIntervalSeconds) return true;
  elapsed_ = 0;
  if (!document.dirty()) return save_or_report(document, error);
  if (written_version_ == document.version()) return true;
  return save_or_report(document, error);
}
}  // namespace redgem
