#include "editor/glb_file.hpp"

#include <cstring>
#include <fstream>
#include <iterator>

namespace redgem {
namespace {

// Binary glTF 2.0 layout: 12-byte header, then chunks of (length, type, data).
constexpr std::uint32_t kGlbMagic = 0x46546C67;      // "glTF"
constexpr std::uint32_t kGlbVersion = 2;
constexpr std::uint32_t kChunkJson = 0x4E4F534A;     // "JSON"
constexpr std::uint32_t kChunkBin = 0x004E4942;      // "BIN\0"
constexpr std::size_t kHeaderBytes = 12;
constexpr std::size_t kChunkHeaderBytes = 8;
constexpr std::size_t kChunkAlignment = 4;
constexpr char kJsonPadding = ' ';
constexpr std::uint8_t kBinPadding = 0;
constexpr const char* kTempSuffix = ".tmp";
constexpr int kCompactIndent = -1;
constexpr bool kAsciiOnly = false;

std::uint32_t read_u32(const std::vector<std::uint8_t>& data, std::size_t offset) {
  std::uint32_t value = 0;
  std::memcpy(&value, data.data() + offset, sizeof(value));
  return value;
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(value));
}

// The whole file as bytes. `json` and `bin` are already padded to the chunk alignment.
std::vector<std::uint8_t> glb_bytes(const std::string& json, const std::vector<std::uint8_t>& bin) {
  std::vector<std::uint8_t> out;
  const std::size_t total =
      kHeaderBytes + kChunkHeaderBytes + json.size() + (bin.empty() ? 0 : kChunkHeaderBytes + bin.size());
  append_u32(out, kGlbMagic);
  append_u32(out, kGlbVersion);
  append_u32(out, static_cast<std::uint32_t>(total));
  append_u32(out, static_cast<std::uint32_t>(json.size()));
  append_u32(out, kChunkJson);
  out.insert(out.end(), json.begin(), json.end());
  if (bin.empty()) return out;
  append_u32(out, static_cast<std::uint32_t>(bin.size()));
  append_u32(out, kChunkBin);
  out.insert(out.end(), bin.begin(), bin.end());
  return out;
}

// Writes to a temporary file first, so a failed save never leaves half a scene behind.
bool write_atomically(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes, std::string& error) {
  const std::filesystem::path temp = path.string() + kTempSuffix;
  std::error_code fs_error;
  {
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    if (!file || !file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
      error = "cannot write " + temp.string();
      file.close();
      std::filesystem::remove(temp, fs_error);
      return false;
    }
  }
  std::filesystem::rename(temp, path, fs_error);
  if (fs_error) {
    error = "cannot replace " + path.string() + ": " + fs_error.message();
    std::error_code cleanup;
    std::filesystem::remove(temp, cleanup);
    return false;
  }
  return true;
}

bool is_glb(const std::vector<std::uint8_t>& data) {
  return data.size() >= kHeaderBytes && read_u32(data, 0) == kGlbMagic &&
         read_u32(data, sizeof(std::uint32_t)) == kGlbVersion;
}

// Reads the chunk whose data starts at `begin` into `glb`. False when its JSON cannot be parsed.
bool read_chunk(const std::vector<std::uint8_t>& data, std::uint32_t type, std::size_t begin, std::size_t length,
                GlbFile& glb, bool& has_json) {
  const auto first = data.begin() + static_cast<std::ptrdiff_t>(begin);
  const auto last = data.begin() + static_cast<std::ptrdiff_t>(begin + length);
  if (type == kChunkBin) glb.bin.assign(first, last);
  if (type != kChunkJson) return true;
  glb.json = nlohmann::json::parse(first, last, nullptr, false);
  if (glb.json.is_discarded()) return false;
  has_json = true;
  return true;
}

// Every chunk of the file. Returns what is wrong with it, or an empty text.
std::string read_chunks(const std::vector<std::uint8_t>& data, GlbFile& glb) {
  bool has_json = false;
  std::size_t offset = kHeaderBytes;
  while (offset + kChunkHeaderBytes <= data.size()) {
    const std::uint32_t length = read_u32(data, offset);
    const std::uint32_t type = read_u32(data, offset + sizeof(std::uint32_t));
    const std::size_t begin = offset + kChunkHeaderBytes;
    if (begin + length > data.size()) return " has a truncated chunk";
    if (!read_chunk(data, type, begin, length, glb, has_json)) return " has invalid JSON";
    offset = begin + glb_padded_size(length);
  }
  return has_json ? "" : " has no JSON chunk";
}

}  // namespace

std::size_t glb_padded_size(std::size_t size) { return (size + kChunkAlignment - 1) / kChunkAlignment * kChunkAlignment; }

std::optional<GlbFile> read_glb(const std::filesystem::path& path, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot open " + path.string();
    return std::nullopt;
  }
  const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (!is_glb(data)) {
    error = path.string() + " is not a glTF 2.0 binary file";
    return std::nullopt;
  }
  GlbFile glb;
  const std::string problem = read_chunks(data, glb);
  if (!problem.empty()) {
    error = path.string() + problem;
    return std::nullopt;
  }
  return glb;
}

bool write_glb(const std::filesystem::path& path, const GlbFile& glb, std::string& error) {
  // Invalid UTF-8 (e.g. a truncated name) is replaced instead of throwing.
  std::string json = glb.json.dump(kCompactIndent, kJsonPadding, kAsciiOnly, nlohmann::json::error_handler_t::replace);
  json.append(glb_padded_size(json.size()) - json.size(), kJsonPadding);
  std::vector<std::uint8_t> bin = glb.bin;
  bin.resize(glb_padded_size(bin.size()), kBinPadding);
  return write_atomically(path, glb_bytes(json, bin), error);
}

}  // namespace redgem
