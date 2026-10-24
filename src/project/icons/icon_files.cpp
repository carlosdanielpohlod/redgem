// The files systems keep icons in: the Mac's .icns, and Windows' .ico (beside a program that
// cannot carry its icon inside), both holding PNGs.

#include <algorithm>
#include <array>
#include <iterator>
#include <cstring>
#include <utility>

#include "project/icons/icon_set.hpp"

namespace redgem {
namespace {

constexpr std::size_t kByte = 1;
constexpr std::size_t kShort = 2;
constexpr std::size_t kWord = 4;

// .ico: a header, one entry a picture, then the pictures (little-endian).
constexpr std::uint32_t kIcoType = 1;  // an icon (2 would be a cursor)
constexpr std::size_t kIcoHeaderBytes = 6;
constexpr std::size_t kIcoEntryBytes = 16;
constexpr int kLargestIcoByteSide = 255;  // a side 256 is written as 0: it does not fit a byte
constexpr std::uint32_t kIcoPlanes = 1;
constexpr std::uint32_t kIcoBitsPerPixel = 32;  // what the PNGs are: RGBA, 8 bits each

// .icns: "icns" and the whole file's length, then each picture after its type and its own length
// (big-endian; each length counts its 8 bytes of header too).
constexpr const char* kIcnsMagic = "icns";
constexpr std::size_t kIcnsTypeBytes = 4;
constexpr std::size_t kIcnsHeaderBytes = kIcnsTypeBytes + kWord;
// The type of each size Apple reads as a PNG.
constexpr std::array<std::pair<int, const char*>, 7> kIcnsTypes = {
    {{16, "icp4"}, {32, "icp5"}, {64, "icp6"}, {128, "ic07"}, {256, "ic08"}, {512, "ic09"}, {1024, "ic10"}}};

std::uint32_t ico_byte_side(int side) { return side > kLargestIcoByteSide ? 0 : static_cast<std::uint32_t>(side); }

void append_ico_entry(bytes::Bytes& out, int side, std::size_t size, std::size_t offset) {
  bytes::append_little(out, ico_byte_side(side), kByte);
  bytes::append_little(out, ico_byte_side(side), kByte);
  bytes::append_little(out, 0, kShort);  // no palette, and reserved
  bytes::append_little(out, kIcoPlanes, kShort);
  bytes::append_little(out, kIcoBitsPerPixel, kShort);
  bytes::append_little(out, static_cast<std::uint32_t>(size), kWord);
  bytes::append_little(out, static_cast<std::uint32_t>(offset), kWord);
}

const char* icns_type(int side) {
  for (const auto& [known, type] : kIcnsTypes) {
    if (known == side) return type;
  }
  return nullptr;
}

void append_text(bytes::Bytes& out, const char* text) { out.insert(out.end(), text, text + std::strlen(text)); }

}  // namespace

bytes::Bytes ico_file(const IconSet& icons) {
  bytes::Bytes out;
  bytes::append_little(out, 0, kShort);  // reserved
  bytes::append_little(out, kIcoType, kShort);
  bytes::append_little(out, static_cast<std::uint32_t>(icons.size()), kShort);
  std::size_t offset = kIcoHeaderBytes + kIcoEntryBytes * icons.size();
  for (const auto& [side, png] : icons) {
    append_ico_entry(out, side, png.size(), offset);
    offset += png.size();
  }
  for (const auto& [side, png] : icons) std::copy(png.begin(), png.end(), std::back_inserter(out));
  return out;
}

bytes::Bytes icns_file(const IconSet& icons) {
  bytes::Bytes out;
  append_text(out, kIcnsMagic);
  bytes::append_big(out, 0, kWord);  // the whole file's length, once it is known
  for (const auto& [side, png] : icons) {
    const char* type = icns_type(side);
    if (type == nullptr) continue;
    append_text(out, type);
    bytes::append_big(out, static_cast<std::uint32_t>(kIcnsHeaderBytes + png.size()), kWord);
    std::copy(png.begin(), png.end(), std::back_inserter(out));
  }
  bytes::write_big(out, kIcnsTypeBytes, static_cast<std::uint32_t>(out.size()), kWord);
  return out;
}

}  // namespace redgem
