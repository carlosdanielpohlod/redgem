// The file the Mac keeps an app's icon in: an .icns holding PNGs.

#include <algorithm>
#include <array>
#include <iterator>
#include <cstring>
#include <utility>

#include "project/icons/icon_set.hpp"

namespace redgem {
namespace {

constexpr std::size_t kWord = 4;

// .icns: "icns" and the whole file's length, then each picture after its type and its own length
// (big-endian; each length counts its 8 bytes of header too).
constexpr const char* kIcnsMagic = "icns";
constexpr std::size_t kIcnsTypeBytes = 4;
constexpr std::size_t kIcnsHeaderBytes = kIcnsTypeBytes + kWord;
// The type of each size Apple reads as a PNG.
constexpr std::array<std::pair<int, const char*>, 7> kIcnsTypes = {
    {{16, "icp4"}, {32, "icp5"}, {64, "icp6"}, {128, "ic07"}, {256, "ic08"}, {512, "ic09"}, {1024, "ic10"}}};

const char* icns_type(int side) {
  for (const auto& [known, type] : kIcnsTypes) {
    if (known == side) return type;
  }
  return nullptr;
}

void append_text(bytes::Bytes& out, const char* text) { out.insert(out.end(), text, text + std::strlen(text)); }

}  // namespace

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
