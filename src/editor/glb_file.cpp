#include "editor/glb_file.hpp"

#include <cstring>
#include <fstream>
#include <iterator>

namespace redgem {
namespace {

// Binary glTF 2.0 layout: 12-byte header, then chunks of (length, type, data).
constexpr std::uint32_t kGlbMagic = 0x46546C67;  // "glTF"
constexpr std::uint32_t kGlbVersion = 2;
constexpr std::uint32_t kChunkJson = 0x4E4F534A;  // "JSON"
constexpr std::uint32_t kChunkBin = 0x004E4942;   // "BIN\0"
constexpr std::size_t kHeaderBytes = 12;
constexpr std::size_t kChunkHeaderBytes = 8;
constexpr std::size_t kChunkAlignment = 4;
constexpr std::uint64_t kMaxGlbBytes = 0xFFFFFFFFull;  // what the 32 bits of a length can say: 4 GB less a byte
constexpr char kJsonPadding = ' ';
constexpr std::uint8_t kBinPadding = 0;
constexpr const char* kTempSuffix = ".tmp";
constexpr int kCompactIndent = -1;
constexpr bool kAsciiOnly = false;
// How deep a file's JSON may be nested. glTF itself goes a handful of levels down; walking,
// copying and writing a document takes stack for every level, so a file does not get to choose more.
constexpr int kMaxJsonDepth = 64;

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

// False when the bytes could not all be written to a new file at `path`.
bool write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  return file && file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void remove_if_there(const std::filesystem::path& path) {
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
}

// Writes to a temporary file first, so a failed save never leaves half a scene behind.
bool write_atomically(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes, std::string& error) {
  const std::filesystem::path temp = path.string() + kTempSuffix;
  if (!write_bytes(temp, bytes)) {
    error = "cannot write " + temp.string();
    remove_if_there(temp);
    return false;
  }
  std::error_code fs_error;
  std::filesystem::rename(temp, path, fs_error);
  if (!fs_error) return true;
  error = "cannot replace " + path.string() + ": " + fs_error.message();
  remove_if_there(temp);
  return false;
}

std::optional<std::vector<std::uint8_t>> file_bytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool is_glb(const std::vector<std::uint8_t>& data) {
  return data.size() >= kHeaderBytes && read_u32(data, 0) == kGlbMagic &&
         read_u32(data, sizeof(std::uint32_t)) == kGlbVersion;
}

// A chunk's JSON; a discarded value when it is not JSON, or nested deeper than kMaxJsonDepth.
template <typename Iterator>
nlohmann::json parse_shallow_json(Iterator first, Iterator last) {
  bool too_deep = false;
  const nlohmann::json::parser_callback_t keep_shallow = [&too_deep](int depth, nlohmann::json::parse_event_t,
                                                                     nlohmann::json&) {
    too_deep = too_deep || depth > kMaxJsonDepth;
    return !too_deep;  // (what is refused is not built either: nothing deep is ever held)
  };
  nlohmann::json parsed = nlohmann::json::parse(first, last, keep_shallow, false);
  return too_deep ? nlohmann::json(nlohmann::json::value_t::discarded) : parsed;
}

// Reads the chunk whose data starts at `begin` into `glb`. False when its JSON cannot be parsed.
bool read_chunk(const std::vector<std::uint8_t>& data, std::uint32_t type, std::size_t begin, std::size_t length,
                GlbFile& glb, bool& has_json) {
  const auto first = data.begin() + static_cast<std::ptrdiff_t>(begin);
  const auto last = data.begin() + static_cast<std::ptrdiff_t>(begin + length);
  if (type == kChunkBin) glb.bin.assign(first, last);
  if (type != kChunkJson) return true;
  glb.json = parse_shallow_json(first, last);
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
    if (length > data.size() - begin) return " has a truncated chunk";
    if (!read_chunk(data, type, begin, length, glb, has_json)) return " has invalid or too deeply nested JSON";
    offset = begin + glb_padded_size(length);
  }
  return has_json ? "" : " has no JSON chunk";
}

}  // namespace

std::size_t glb_padded_size(std::size_t size) {
  return (size + kChunkAlignment - 1) / kChunkAlignment * kChunkAlignment;
}

bool fits_in_glb(std::uint64_t json_bytes, std::uint64_t bin_bytes) {
  // (Each chunk on its own first: the sum below then stays far from what 64 bits hold.)
  if (json_bytes > kMaxGlbBytes || bin_bytes > kMaxGlbBytes) return false;
  const std::uint64_t bin_chunk = bin_bytes == 0 ? 0 : kChunkHeaderBytes + bin_bytes;
  return kHeaderBytes + kChunkHeaderBytes + json_bytes + bin_chunk <= kMaxGlbBytes;
}

std::optional<GlbFile> read_glb(const std::filesystem::path& path, std::string& error) {
  const auto data = file_bytes(path);
  if (!data) {
    error = "cannot open " + path.string();
    return std::nullopt;
  }
  GlbFile glb;
  const std::string problem = is_glb(*data) ? read_chunks(*data, glb) : " is not a glTF 2.0 binary file";
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
  if (!fits_in_glb(json.size(), glb_padded_size(glb.bin.size()))) {
    error = "cannot write " + path.string() + ": the scene is bigger than a .glb file can be (4 GB)";
    return false;
  }
  std::vector<std::uint8_t> bin = glb.bin;
  bin.resize(glb_padded_size(bin.size()), kBinPadding);
  return write_atomically(path, glb_bytes(json, bin), error);
}

}  // namespace redgem
