#include "core/ktx2_header.hpp"

#include <algorithm>
#include <array>

#include "core/game_archive_layout.hpp"

namespace redgem {
namespace {

using archive_layout::fits;
using archive_layout::kU32Bytes;
using archive_layout::kU64Bytes;
using archive_layout::little_endian;

// The twelve bytes every KTX2 file starts with.
constexpr std::array<std::uint8_t, 12> kKtx2Identifier = {0xAB, 'K',  'T',  'X',  ' ',  '2',
                                                          '0',  0xBB, '\r', '\n', 0x1A, '\n'};
// Where the header's fields are (each a 32-bit number).
constexpr std::size_t kVkFormatAt = 12;
constexpr std::size_t kWidthAt = 20;
constexpr std::size_t kHeightAt = 24;
constexpr std::size_t kDepthAt = 28;
constexpr std::size_t kLayerCountAt = 32;
constexpr std::size_t kFaceCountAt = 36;
constexpr std::size_t kLevelCountAt = 40;
constexpr std::size_t kSupercompressionAt = 44;
// The level index follows the header: per level, its offset, length and uncompressed length.
constexpr std::size_t kLevelIndexAt = 80;
constexpr std::size_t kLevelEntryBytes = 24;
constexpr std::size_t kLevelLengthAt = 8;  // from the entry's start
// Basis data has no Vulkan format of its own (VK_FORMAT_UNDEFINED).
constexpr std::uint32_t kBasisVkFormat = 0;
constexpr std::uint32_t kOneFace = 1;  // not a cube map
constexpr std::uint32_t kLastSupercompression = static_cast<std::uint32_t>(Ktx2Supercompression::Zstandard);

std::uint32_t field(const std::uint8_t* data, std::size_t at) {
  return static_cast<std::uint32_t>(little_endian(data + at, kU32Bytes));
}

// A 2D picture of Basis data: neither a volume, an array of pictures nor a cube map.
bool one_basis_picture(const std::uint8_t* data) {
  const bool flat = field(data, kDepthAt) == 0 && field(data, kLayerCountAt) <= 1;
  return flat && field(data, kFaceCountAt) == kOneFace && field(data, kVkFormatAt) == kBasisVkFormat;
}

// How many levels a full chain of mipmaps of a picture this size has, down to one pixel.
std::uint32_t full_chain(ImageSize size) {
  std::uint32_t levels = 1;
  for (std::uint32_t side = std::max(size.width, size.height); side > 1; side >>= 1) ++levels;
  return levels;
}

bool within_limits(ImageSize size) {
  return std::min(size.width, size.height) > 0 && std::max(size.width, size.height) <= kMaxImageSide;
}

// Whether the level of this index entry lies inside the file, with some data.
bool level_inside(const std::uint8_t* entry, std::size_t size) {
  const std::uint64_t length = little_endian(entry + kLevelLengthAt, kU64Bytes);
  return length > 0 && fits(little_endian(entry, kU64Bytes), length, size);
}

// Whether every level the index lists lies inside the file.
bool levels_inside(const std::uint8_t* data, std::size_t size, std::uint32_t levels) {
  if (!fits(kLevelIndexAt, std::uint64_t{levels} * kLevelEntryBytes, size)) return false;
  for (std::uint32_t level = 0; level < levels; ++level) {
    if (!level_inside(data + kLevelIndexAt + level * kLevelEntryBytes, size)) return false;
  }
  return true;
}

// The header's claims, before they are checked against the file.
Ktx2Header claimed_header(const std::uint8_t* data) {
  Ktx2Header header;
  header.size = ImageSize{field(data, kWidthAt), field(data, kHeightAt)};
  // (No levels means the reader makes its own mipmaps: only the full-size picture is stored.)
  header.levels = std::max(field(data, kLevelCountAt), std::uint32_t{1});
  header.supercompression = static_cast<Ktx2Supercompression>(field(data, kSupercompressionAt));
  return header;
}

bool known_supercompression(const std::uint8_t* data) {
  return field(data, kSupercompressionAt) <= kLastSupercompression;
}

// What the header says, before its numbers are believed: a picture the engine draws, stored in a
// way it reads.
bool drawable_kind(const std::uint8_t* data) { return one_basis_picture(data) && known_supercompression(data); }

bool within_file_limit(std::size_t size) { return size <= kMaxEncodedImageBytes; }

bool plausible(const Ktx2Header& header) {
  return within_limits(header.size) && header.levels <= full_chain(header.size);
}

}  // namespace

bool starts_as_ktx2(const std::uint8_t* data, std::size_t size) {
  if (data == nullptr) return false;
  if (size < kLevelIndexAt) return false;
  return std::equal(kKtx2Identifier.begin(), kKtx2Identifier.end(), data);
}

std::optional<Ktx2Header> read_ktx2_header(const std::uint8_t* data, std::size_t size) {
  if (!within_file_limit(size)) return std::nullopt;
  if (!starts_as_ktx2(data, size)) return std::nullopt;
  if (!drawable_kind(data)) return std::nullopt;
  const Ktx2Header header = claimed_header(data);
  if (!plausible(header)) return std::nullopt;
  if (!levels_inside(data, size, header.levels)) return std::nullopt;
  return header;
}

}  // namespace redgem
