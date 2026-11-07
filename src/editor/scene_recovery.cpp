#include "editor/scene_recovery.hpp"

#include <mbedtls/sha256.h>
#include <array>
#include <fstream>
#include <stdexcept>

#include "storage/durable_file.hpp"
#include "editor/document/recovery_format.hpp"
#include "scene/scene_ids.hpp"

namespace redgem {
namespace {
namespace fs = std::filesystem;
constexpr const char* kSuffix = ".recovery";
constexpr const char* kStagePrefix = ".redgem-recovery-";
constexpr const char* kStageFile = "copy";
using recovery_format::kMetadata;
constexpr const char* kSource = "source";
constexpr const char* kBaseline = "baseline";
constexpr const char* kVersion = "version";
constexpr int kFormatVersion = 1;
constexpr std::size_t kChunkBytes = 64 * 1024;
constexpr std::size_t kDigestBytes = 32;
constexpr char kHex[] = "0123456789abcdef";
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
void safe_copy(const fs::path& source, const fs::path& copy) {
  require(!fs::is_symlink(fs::symlink_status(source)), "scene recovery source is a symbolic link");
  require(!fs::is_symlink(fs::symlink_status(copy)), "recovery copy is a symbolic link");
}
std::optional<GlbFile> checked_copy(const fs::path& source, std::string& error) {
  const auto copy = SceneRecovery::copy_path(source);
  safe_copy(source, copy);
  require(fs::file_size(copy) <= recovery_format::kMaxBytes, "recovery copy exceeds the 64 MiB limit");
  auto glb = read_glb(copy, error);
  if (!glb) return std::nullopt;
  const auto& meta = glb->json.at(kMetadata);
  require(meta.at(kVersion) == kFormatVersion, "unknown recovery format");
  require(meta.at(kSource) == source.filename().generic_u8string(), "recovery belongs to another scene");
  require(meta.at(kBaseline).is_string(), "invalid recovery source fingerprint");
  return glb;
}
std::optional<SceneDocument> read_recovered(const fs::path& source, std::string& error) {
  if (!checked_copy(source, error)) return std::nullopt;
  auto opened = SceneDocument::open(SceneRecovery::copy_path(source));
  if (auto* problem = std::get_if<std::string>(&opened)) {
    error = *problem;
    return std::nullopt;
  }
  auto document = std::move(std::get<SceneDocument>(opened));
  document.recovered_as(source);
  return document;
}
struct Stage {
  fs::path folder;
  explicit Stage(const fs::path& parent) : folder(parent / (std::string(kStagePrefix) + make_scene_id())) {
    require(fs::create_directory(folder), "cannot create recovery staging folder");
  }
  ~Stage() {
    std::error_code ignored;
    fs::remove_all(folder, ignored);
  }
  Stage(const Stage&) = delete;
  Stage& operator=(const Stage&) = delete;
};
bool publish_copy(const GlbFile& glb, const fs::path& copy, const durable::Flush& flush, std::string& error) {
  const auto bytes = encode_glb(glb, error);
  if (!bytes) return false;
  require(bytes->size() <= recovery_format::kMaxBytes, "recovery copy exceeds the 64 MiB limit");
  const Stage stage(copy.parent_path());
  const auto staged = stage.folder / kStageFile;
  require(durable::write_flushed(staged, std::string(bytes->begin(), bytes->end()), flush),
          "cannot write and flush scene recovery");
  fs::rename(staged, copy);
  require(durable::sync_folder(copy.parent_path()), "recovery was written but its folder could not be flushed");
  return true;
}
}  // namespace
fs::path SceneRecovery::copy_path(const fs::path& source) {
  return source.parent_path() / fs::u8path("." + source.filename().generic_u8string() + kSuffix);
}
void SceneRecovery::watch(const fs::path& source) {
  source_changed_.reset();
  source_ = source;
  baseline_ = fingerprint(source);
  written_version_.reset();
  elapsed_ = 0;
  std::error_code error;
  pending_ = fs::exists(fs::symlink_status(copy_path(source), error));
}
bool SceneRecovery::source_changed() const {
  if (source_changed_) return *source_changed_;
  try {
    std::string error;
    const auto copy = checked_copy(source_, error);
    source_changed_ = !copy || copy->json.at(kMetadata).at(kBaseline) != fingerprint(source_);
  } catch (const std::exception&) {
    source_changed_ = true;
  }
  return *source_changed_;
}
bool SceneRecovery::recover(SceneDocument& document, std::string& error) {
  try {
    auto recovered = read_recovered(source_, error);
    if (!recovered) return false;
    document = std::move(*recovered);
    pending_ = false;
    return true;
  } catch (const std::exception& problem) {
    error = problem.what();
    return false;
  }
}
bool SceneRecovery::discard(std::string& error) {
  std::error_code problem;
  fs::remove(copy_path(source_), problem);
  if (problem) {
    error = "cannot remove scene recovery: " + problem.message();
    return false;
  }
  pending_ = false;
  written_version_.reset();
  return true;
}
bool SceneRecovery::write(SceneDocument& document, std::string& error) {
  const auto copy = copy_path(source_);
  safe_copy(source_, copy);
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
    return true;
  } catch (const std::exception& problem) {
    error = problem.what();
    return false;
  }
}
bool SceneRecovery::poll(SceneDocument& document, float seconds, std::string& error) {
  if (pending_) return true;
  elapsed_ += seconds;
  if (elapsed_ < kIntervalSeconds) return true;
  elapsed_ = 0;
  if (!document.dirty()) return discard(error);
  if (written_version_ == document.version()) return true;
  return save_version(document, error);
}
}  // namespace redgem
