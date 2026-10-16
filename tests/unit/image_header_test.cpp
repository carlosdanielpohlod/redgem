#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "core/image_header.hpp"

using namespace redgem;

namespace {

constexpr std::uint32_t kWidth = 640;
constexpr std::uint32_t kHeight = 480;
constexpr std::uint32_t kTooWide = kMaxImageSide + 1;
constexpr int kBitsPerByte = 8;

void put_big_endian(std::vector<std::uint8_t>& out, std::uint32_t value, int bytes) {
  for (int shift = (bytes - 1) * kBitsPerByte; shift >= 0; shift -= kBitsPerByte) {
    out.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

// The start of a PNG file saying its picture is this big.
std::vector<std::uint8_t> png_header(std::uint32_t width, std::uint32_t height) {
  std::vector<std::uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n', 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
  put_big_endian(out, width, 4);
  put_big_endian(out, height, 4);
  out.insert(out.end(), {8, 6, 0, 0, 0});
  return out;
}

// The start of a JPEG file: an application segment, a table, then the frame of this size.
std::vector<std::uint8_t> jpeg_header(std::uint32_t width, std::uint32_t height) {
  std::vector<std::uint8_t> out = {0xFF, 0xD8, 0xFF, 0xE0, 0,    4,    'J', 'F', 0xFF, 0xC4,
                                   0,    3,    0,    0xFF, 0xFF, 0xC2, 0,   11,  8};
  put_big_endian(out, height, 2);
  put_big_endian(out, width, 2);
  out.insert(out.end(), {3, 1, 0x11, 0});
  return out;
}

}  // namespace

TEST_CASE("the size of a PNG and of a JPEG is read from its first bytes") {
  const std::vector<std::uint8_t> png = png_header(kWidth, kHeight);
  const auto png_size = encoded_image_size(png.data(), png.size());
  REQUIRE(png_size.has_value());
  CHECK(png_size->width == kWidth);
  CHECK(png_size->height == kHeight);

  const std::vector<std::uint8_t> jpeg = jpeg_header(kWidth, kHeight);
  const auto jpeg_size = encoded_image_size(jpeg.data(), jpeg.size());
  REQUIRE(jpeg_size.has_value());
  CHECK(jpeg_size->width == kWidth);
  CHECK(jpeg_size->height == kHeight);
}

TEST_CASE("what is not a PNG or JPEG, or is cut short, has no size") {
  const std::vector<std::uint8_t> bitmap = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  CHECK_FALSE(encoded_image_size(bitmap.data(), bitmap.size()).has_value());
  CHECK_FALSE(encoded_image_size(nullptr, 0).has_value());
  std::vector<std::uint8_t> png = png_header(kWidth, kHeight);
  png.resize(18);
  CHECK_FALSE(encoded_image_size(png.data(), png.size()).has_value());
  std::vector<std::uint8_t> jpeg = jpeg_header(kWidth, kHeight);
  jpeg.resize(jpeg.size() - 6);
  CHECK_FALSE(encoded_image_size(jpeg.data(), jpeg.size()).has_value());
  // A segment that says it is longer than the file.
  const std::vector<std::uint8_t> lying = {0xFF, 0xD8, 0xFF, 0xE0, 0xFF, 0xFF, 0, 0};
  CHECK_FALSE(encoded_image_size(lying.data(), lying.size()).has_value());
  // Picture data before any frame.
  const std::vector<std::uint8_t> scan_first = {0xFF, 0xD8, 0xFF, 0xDA, 0, 2, 0, 0};
  CHECK_FALSE(encoded_image_size(scan_first.data(), scan_first.size()).has_value());
}

TEST_CASE("a picture is decoded only when its header says it is within the limits") {
  const std::vector<std::uint8_t> fine = png_header(kMaxImageSide, kMaxImageSide);
  CHECK(decodable_image(fine.data(), fine.size()));
  const std::vector<std::uint8_t> wide = png_header(kTooWide, 1);
  CHECK_FALSE(decodable_image(wide.data(), wide.size()));
  const std::vector<std::uint8_t> tall = jpeg_header(1, kTooWide);
  CHECK_FALSE(decodable_image(tall.data(), tall.size()));
  const std::vector<std::uint8_t> huge = png_header(0xFFFFFFFF, 0xFFFFFFFF);
  CHECK_FALSE(decodable_image(huge.data(), huge.size()));
  const std::vector<std::uint8_t> empty = png_header(0, kHeight);
  CHECK_FALSE(decodable_image(empty.data(), empty.size()));
  const std::vector<std::uint8_t> bitmap = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  CHECK_FALSE(decodable_image(bitmap.data(), bitmap.size()));
  // More encoded bytes than are ever decoded (only the size is looked at).
  CHECK_FALSE(decodable_image(fine.data(), kMaxEncodedImageBytes + 1));
}
