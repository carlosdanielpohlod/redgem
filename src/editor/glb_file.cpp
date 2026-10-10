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

std::size_t padded(std::size_t size) { return (size + kChunkAlignment - 1) / kChunkAlignment * kChunkAlignment; }

}  // namespace

std::optional<GlbFile> read_glb(const std::filesystem::path& path, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot open " + path.string();
    return std::nullopt;
  }
  const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (data.size() < kHeaderBytes || read_u32(data, 0) != kGlbMagic || read_u32(data, sizeof(std::uint32_t)) != kGlbVersion) {
    error = path.string() + " is not a glTF 2.0 binary file";
    return std::nullopt;
  }
  GlbFile glb;
  bool has_json = false;
  std::size_t offset = kHeaderBytes;
  while (offset + kChunkHeaderBytes <= data.size()) {
    const std::uint32_t length = read_u32(data, offset);
    const std::uint32_t type = read_u32(data, offset + sizeof(std::uint32_t));
    const std::size_t begin = offset + kChunkHeaderBytes;
    if (begin + length > data.size()) {
      error = path.string() + " has a truncated chunk";
      return std::nullopt;
    }
    if (type == kChunkJson) {
      glb.json = nlohmann::json::parse(data.begin() + static_cast<std::ptrdiff_t>(begin),
                                       data.begin() + static_cast<std::ptrdiff_t>(begin + length), nullptr, false);
      if (glb.json.is_discarded()) {
        error = path.string() + " has invalid JSON";
        return std::nullopt;
      }
      has_json = true;
    } else if (type == kChunkBin) {
      glb.bin.assign(data.begin() + static_cast<std::ptrdiff_t>(begin),
                     data.begin() + static_cast<std::ptrdiff_t>(begin + length));
    }
    offset = begin + padded(length);
  }
  if (!has_json) {
    error = path.string() + " has no JSON chunk";
    return std::nullopt;
  }
  return glb;
}

bool write_glb(const std::filesystem::path& path, const GlbFile& glb, std::string& error) {
  // Invalid UTF-8 (e.g. a truncated name) is replaced instead of throwing.
  std::string json = glb.json.dump(kCompactIndent, kJsonPadding, kAsciiOnly, nlohmann::json::error_handler_t::replace);
  json.append(padded(json.size()) - json.size(), kJsonPadding);
  std::vector<std::uint8_t> bin = glb.bin;
  bin.resize(padded(bin.size()), kBinPadding);

  std::vector<std::uint8_t> out;
  const std::size_t total =
      kHeaderBytes + kChunkHeaderBytes + json.size() + (bin.empty() ? 0 : kChunkHeaderBytes + bin.size());
  append_u32(out, kGlbMagic);
  append_u32(out, kGlbVersion);
  append_u32(out, static_cast<std::uint32_t>(total));
  append_u32(out, static_cast<std::uint32_t>(json.size()));
  append_u32(out, kChunkJson);
  out.insert(out.end(), json.begin(), json.end());
  if (!bin.empty()) {
    append_u32(out, static_cast<std::uint32_t>(bin.size()));
    append_u32(out, kChunkBin);
    out.insert(out.end(), bin.begin(), bin.end());
  }

  const std::filesystem::path temp = path.string() + kTempSuffix;
  std::error_code fs_error;
  {
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    if (!file || !file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()))) {
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

}  // namespace redgem
