#include "project/editor_bridge_wire.hpp"

#include <algorithm>
#include <cctype>
#include <array>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

#include <mbedtls/md.h>

#include "core/files.hpp"
#include "core/platform.hpp"
#include "core/safe_path.hpp"

namespace redgem::tools::bridge {
namespace {
constexpr std::size_t kTokenBytes = 32;
constexpr int kByteMaximum = 255;
constexpr int kHexWidth = 2;
constexpr int kMaxDepth = 16;
constexpr std::uintmax_t kDescriptorBytes = 4096;
constexpr int kLargestPort = 65535;
// What the editor's proof is a keyed hash of, besides the client's nonce: no other use of the
// token can produce the same bytes.
constexpr const char* kProofContext = "redgem editor bridge proof v2\n";
constexpr std::size_t kSha256Bytes = 32;

std::string hex(const unsigned char* bytes, std::size_t count) {
  std::ostringstream text;
  text << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < count; ++i) text << std::setw(kHexWidth) << static_cast<int>(bytes[i]);
  return text.str();
}

bool hex_digit(char letter) { return std::isxdigit(static_cast<unsigned char>(letter)) != 0; }

constexpr const char* kBridgeFolder = "editor-bridge";
constexpr const char* kConnectionName = "connection.json";

std::filesystem::path bridge_root(const std::filesystem::path& root) {
  return root.empty() ? platform::user_data_dir() / kBridgeFolder : root;
}

std::filesystem::path private_directory(const std::filesystem::path& root) {
  const auto parent = bridge_root(root);
  std::filesystem::create_directories(parent);
  const auto folder = parent / random_token();
  if (!std::filesystem::create_directory(folder)) throw std::runtime_error("cannot create private bridge directory");
#ifndef _WIN32
  std::filesystem::permissions(folder, std::filesystem::perms::owner_all);
#endif
  return folder;
}

Json descriptor(const Address& address) {
  return {{"version", kWireVersion},
          {"game", address.game.generic_u8string()},
          {"port", address.port},
          {"token", address.token}};
}

Address decode_address(const Json& json) {
  if (!json.is_object() || json.value("version", Json()) != kWireVersion)
    throw std::runtime_error("invalid bridge descriptor");
  const auto port = json.value("port", Json());
  if (!port.is_number_integer()) throw std::runtime_error("invalid bridge port");
  if (port < 1 || port > kLargestPort) throw std::runtime_error("invalid bridge port");
  if (!json.value("game", Json()).is_string()) throw std::runtime_error("invalid bridge game directory");
  const auto token = json.value("token", Json());
  if (!token.is_string() || token.get_ref<const std::string&>().size() != kTokenBytes * kHexWidth)
    throw std::runtime_error("invalid bridge credential");
  return {std::filesystem::u8path(json["game"].get<std::string>()), port.get<std::uint16_t>(),
          token.get<std::string>()};
}
}  // namespace

std::string random_token() {
  std::random_device source;
  std::uniform_int_distribution<int> byte(0, kByteMaximum);
  std::ostringstream token;
  token << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < kTokenBytes; ++i) token << std::setw(kHexWidth) << byte(source);
  return token.str();
}

bool same_token(const std::string& expected, const Json& given) {
  if (!given.is_string()) return false;
  const auto& text = given.get_ref<const std::string&>();
  if (text.size() != expected.size()) return false;
  volatile unsigned char different = 0;
  for (std::size_t i = 0; i < text.size(); ++i)
    different = static_cast<unsigned char>(different | (text[i] ^ expected[i]));
  return different == 0;
}

ConnectionFile::ConnectionFile(const Address& address, const std::filesystem::path& parent)
    : directory_(private_directory(parent)), path_(directory_ / kConnectionName) {
  try {
    std::string error;
    if (!write_text_file(path_, descriptor(address).dump(), error))
      throw std::runtime_error("cannot write bridge descriptor");
#ifndef _WIN32
    std::filesystem::permissions(path_, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
#endif
  } catch (...) {
    std::filesystem::remove_all(directory_);
    throw;
  }
}
ConnectionFile::~ConnectionFile() {
  std::error_code ignored;
  std::filesystem::remove_all(directory_, ignored);
}

Address read_address(const std::filesystem::path& game, const std::filesystem::path& path) {
  const auto bytes = read_file_up_to(path, kDescriptorBytes);
  if (!bytes) throw std::runtime_error("cannot read bridge connection file");
  Address address;
  try {
    address = decode_address(parse(std::string(bytes->begin(), bytes->end())));
  } catch (const std::exception&) {
    throw std::runtime_error("invalid bridge connection file");
  }
  if (!std::filesystem::equivalent(game, address.game)) throw std::runtime_error("bridge belongs to another game");
  return address;
}

namespace {
using Dated = std::pair<std::filesystem::file_time_type, std::filesystem::path>;

void add_connection(const std::filesystem::directory_entry& folder, std::vector<Dated>& found) {
  std::error_code error;
  const auto file = folder.path() / kConnectionName;
  const auto written = std::filesystem::last_write_time(file, error);
  if (!error) found.emplace_back(written, file);
}
}  // namespace

std::vector<std::filesystem::path> connection_files(const std::filesystem::path& root) {
  std::vector<Dated> found;
  std::error_code error;
  for (const auto& folder : std::filesystem::directory_iterator(bridge_root(root), error))
    add_connection(folder, found);
  std::sort(found.begin(), found.end(), [](const Dated& a, const Dated& b) { return a.first > b.first; });
  std::vector<std::filesystem::path> files;
  files.reserve(found.size());
  for (auto& dated : found) files.push_back(std::move(dated.second));
  return files;
}

bool belongs_to(const std::filesystem::path& game, const std::filesystem::path& file) {
  try {
    read_address(game, file);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

std::string take_line(Bytes& bytes) {
  const auto end = std::find(bytes.begin(), bytes.end(), '\n');
  std::string line(bytes.begin(), end);
  bytes.erase(bytes.begin(), end == bytes.end() ? end : end + 1);
  return line;
}

std::string server_proof(const std::string& token, const std::string& nonce) {
  const std::string message = kProofContext + nonce;
  std::array<unsigned char, kSha256Bytes> digest{};
  const auto* sha256 = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  const int failed =
      mbedtls_md_hmac(sha256, reinterpret_cast<const unsigned char*>(token.data()), token.size(),
                      reinterpret_cast<const unsigned char*>(message.data()), message.size(), digest.data());
  if (failed != 0) throw std::runtime_error("cannot compute the bridge proof");
  return hex(digest.data(), digest.size());
}

bool valid_nonce(const Json& nonce) {
  if (!nonce.is_string()) return false;
  const auto& text = nonce.get_ref<const std::string&>();
  return text.size() == kTokenBytes * kHexWidth && std::all_of(text.begin(), text.end(), hex_digit);
}

Json parse(const std::string& text) {
  return Json::parse(text, [](int depth, Json::parse_event_t, Json&) {
    if (depth > kMaxDepth) throw std::runtime_error("bridge JSON nesting limit exceeded");
    return true;
  });
}

Bytes encode(const Json& value) {
  const std::string text = value.dump(-1, ' ', false, Json::error_handler_t::replace) + "\n";
  return Bytes(text.begin(), text.end());
}

bool complete(const Bytes& bytes) { return std::find(bytes.begin(), bytes.end(), '\n') != bytes.end(); }
}  // namespace redgem::tools::bridge
