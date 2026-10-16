#include "core/image_header.hpp"

#include <algorithm>
#include <array>

namespace redgem {
namespace {

// A PNG file: eight fixed bytes, then its first chunk, which holds the size.
constexpr std::array<std::uint8_t, 8> kPngSignature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
constexpr std::array<std::uint8_t, 4> kPngHeaderChunk = {'I', 'H', 'D', 'R'};
constexpr std::size_t kPngChunkNameAt = 12;  // after the signature and the chunk's length
constexpr std::size_t kPngWidthAt = 16;
constexpr std::size_t kPngHeightAt = 20;
constexpr std::size_t kPngHeaderEnd = 24;
// A JPEG file: a start mark, then segments, each a 0xFF byte, a kind and (for most) a length.
constexpr std::uint8_t kJpegMark = 0xFF;
constexpr std::uint8_t kJpegStart = 0xD8;
constexpr std::uint8_t kJpegScan = 0xDA;  // the picture's data begins: no size after this
constexpr std::uint8_t kJpegEnd = 0xD9;
constexpr std::uint8_t kFirstFrameKind = 0xC0;  // the segments 0xC0..0xCF that describe the frame...
constexpr std::uint8_t kLastFrameKind = 0xCF;
// ... except these three, which share the range and are tables.
constexpr std::array<std::uint8_t, 3> kTableKinds = {0xC4, 0xC8, 0xCC};
constexpr std::size_t kJpegSegmentsAt = 2;
constexpr std::size_t kSegmentHeaderBytes = 4;  // mark, kind and a two-byte length
constexpr std::size_t kSegmentLengthAt = 2;     // from the segment's mark
constexpr std::size_t kFrameHeightAt = 5;
constexpr std::size_t kFrameWidthAt = 7;
constexpr std::size_t kFrameSizeEnd = 9;
constexpr int kBitsPerByte = 8;

// A number stored most significant byte first, as both formats store theirs.
std::uint32_t big_endian(const std::uint8_t* bytes, std::size_t count) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < count; ++i) value = (value << kBitsPerByte) | bytes[i];
  return value;
}

std::optional<ImageSize> png_size(const std::uint8_t* data, std::size_t size) {
  const bool is_png = size >= kPngHeaderEnd && std::equal(kPngSignature.begin(), kPngSignature.end(), data) &&
                      std::equal(kPngHeaderChunk.begin(), kPngHeaderChunk.end(), data + kPngChunkNameAt);
  if (!is_png) return std::nullopt;
  return ImageSize{big_endian(data + kPngWidthAt, sizeof(std::uint32_t)),
                   big_endian(data + kPngHeightAt, sizeof(std::uint32_t))};
}

bool describes_frame(std::uint8_t kind) {
  return kind >= kFirstFrameKind && kind <= kLastFrameKind &&
         std::find(kTableKinds.begin(), kTableKinds.end(), kind) == kTableKinds.end();
}

// Whether the segment at `at` describes the frame and is all there.
bool frame_at(const std::uint8_t* data, std::size_t size, std::size_t at) {
  return describes_frame(data[at + 1]) && size - at >= kFrameSizeEnd;
}

// The picture's data begins, or the file ends: no segment after this one describes the frame.
bool ends_the_segments(std::uint8_t kind) { return kind == kJpegScan || kind == kJpegEnd; }

// Where the segment after the one at `at` starts; `size` (nothing more to look at) when that one
// is where the picture's data begins, or says it is longer than the file.
std::size_t next_segment(const std::uint8_t* data, std::size_t size, std::size_t at) {
  const std::uint8_t kind = data[at + 1];
  if (kind == kJpegMark) return at + 1;  // marks may be padded with more 0xFF bytes
  if (ends_the_segments(kind)) return size;
  const std::size_t step = kSegmentLengthAt + big_endian(data + at + kSegmentLengthAt, sizeof(std::uint16_t));
  return step > size - at ? size : at + step;
}

bool starts_as_jpeg(const std::uint8_t* data, std::size_t size) {
  return size >= kJpegSegmentsAt && data[0] == kJpegMark && data[1] == kJpegStart;
}

// Whether a segment's header is at `at`, all of it.
bool segment_at(const std::uint8_t* data, std::size_t size, std::size_t at) {
  return size - at >= kSegmentHeaderBytes && data[at] == kJpegMark;
}

// Walks the segments up to the one that describes the frame.
std::optional<ImageSize> jpeg_size(const std::uint8_t* data, std::size_t size) {
  if (!starts_as_jpeg(data, size)) return std::nullopt;
  for (std::size_t at = kJpegSegmentsAt; segment_at(data, size, at); at = next_segment(data, size, at)) {
    if (!frame_at(data, size, at)) continue;
    return ImageSize{big_endian(data + at + kFrameWidthAt, sizeof(std::uint16_t)),
                     big_endian(data + at + kFrameHeightAt, sizeof(std::uint16_t))};
  }
  return std::nullopt;
}

}  // namespace

std::optional<ImageSize> encoded_image_size(const std::uint8_t* data, std::size_t size) {
  if (data == nullptr) return std::nullopt;
  if (const std::optional<ImageSize> png = png_size(data, size)) return png;
  return jpeg_size(data, size);
}

bool decodable_image(const std::uint8_t* data, std::size_t size) {
  if (size > kMaxEncodedImageBytes) return false;
  const std::optional<ImageSize> picture = encoded_image_size(data, size);
  if (!picture) return false;
  const std::uint32_t longest = std::max(picture->width, picture->height);
  return std::min(picture->width, picture->height) > 0 && longest <= kMaxImageSide;
}

}  // namespace redgem
