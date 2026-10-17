// Radiance (.hdr) pictures: what is read of good files, and that malformed ones are refused
// without reading past their end.
#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/image_header.hpp"
#include "core/radiance_image.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint8_t kOneExponent = 129;  // a mantissa of 128 under it is 1.0
constexpr std::uint8_t kHalfMantissa = 64;
constexpr std::uint8_t kFullMantissa = 128;
constexpr std::uint32_t kEncodableWidth = 8;  // the narrowest scanline that may be run-length encoded
constexpr std::uint8_t kRepeat = 128;         // a run count above this repeats one byte
constexpr std::uint32_t kWide = 4;
constexpr std::uint32_t kHigh = 2;
constexpr std::uint32_t kNoLimit = 1024;

Bytes text(const std::string& value) { return Bytes(value.begin(), value.end()); }

Bytes header(std::uint32_t width, std::uint32_t height, const std::string& format = "32-bit_rle_rgbe") {
  return text("#?RADIANCE\nFORMAT=" + format + "\nEXPOSURE=1.0\n\n-Y " + std::to_string(height) + " +X " +
              std::to_string(width) + "\n");
}

// A flat (not encoded) picture whose every pixel is `pixel`.
Bytes flat_picture(std::uint32_t width, std::uint32_t height, const Bytes& pixel) {
  Bytes file = header(width, height);
  for (std::uint32_t i = 0; i < width * height; ++i) file.insert(file.end(), pixel.begin(), pixel.end());
  return file;
}

// One encoded scanline of kEncodableWidth pixels: red repeated, green and blue literal, exponent repeated.
Bytes encoded_scanline() {
  Bytes line = {2, 2, 0, kEncodableWidth};
  line.insert(line.end(), {kRepeat + kEncodableWidth, kFullMantissa});
  for (int channel = 0; channel < 2; ++channel) {
    line.push_back(kEncodableWidth);
    for (std::uint32_t x = 0; x < kEncodableWidth; ++x) line.push_back(static_cast<std::uint8_t>(x * 16));
  }
  line.insert(line.end(), {kRepeat + kEncodableWidth, kOneExponent});
  return line;
}

}  // namespace

TEST_CASE("a flat Radiance picture is read as linear light, rows from the top") {
  Bytes file = header(kWide, kHigh);
  for (std::uint32_t i = 0; i < kWide; ++i) file.insert(file.end(), {kFullMantissa, kHalfMantissa, 0, kOneExponent});
  for (std::uint32_t i = 0; i < kWide; ++i) file.insert(file.end(), {0, 0, 0, 0});
  const auto image = decode_radiance(file.data(), file.size(), kNoLimit);
  REQUIRE(image.has_value());
  CHECK(image->width == kWide);
  CHECK(image->height == kHigh);
  CHECK(approx_equal(image->at(0, 0), Vec3{1.0f, 0.5f, 0.0f}, kTolerance));
  CHECK(approx_equal(image->at(kWide - 1, kHigh - 1), Vec3::zero(), kTolerance));
}

TEST_CASE("run-length encoded scanlines are read channel by channel") {
  Bytes file = header(kEncodableWidth, 1);
  const Bytes line = encoded_scanline();
  file.insert(file.end(), line.begin(), line.end());
  const auto image = decode_radiance(file.data(), file.size(), kNoLimit);
  REQUIRE(image.has_value());
  CHECK(image->at(0, 0).x == doctest::Approx(1.0f));
  CHECK(image->at(kEncodableWidth - 1, 0).y == doctest::Approx(112.0f / 128.0f));
}

TEST_CASE("a large Radiance picture is shrunk while it is read, its blocks averaged") {
  Bytes file = header(kWide, kHigh);
  for (std::uint32_t y = 0; y < kHigh; ++y) {
    for (std::uint32_t x = 0; x < kWide; ++x) {
      const std::uint8_t red = (x + y) % 2 == 0 ? kFullMantissa : 0;
      file.insert(file.end(), {red, 0, 0, kOneExponent});
    }
  }
  const auto image = decode_radiance(file.data(), file.size(), kWide / 2);
  REQUIRE(image.has_value());
  CHECK(image->width == kWide / 2);
  CHECK(image->height == kHigh / 2);
  CHECK(image->at(0, 0).x == doctest::Approx(0.5f));
}

TEST_CASE("headers that are not of a Radiance picture this reads are refused") {
  const Bytes pixel = {0, 0, 0, 0};
  CHECK_FALSE(radiance_layout(nullptr, 0).has_value());
  const Bytes png = {0x89, 'P', 'N', 'G'};
  CHECK_FALSE(radiance_layout(png.data(), png.size()).has_value());
  const Bytes xyz = header(kWide, kHigh, "32-bit_rle_xyze");
  CHECK_FALSE(radiance_layout(xyz.data(), xyz.size()).has_value());
  const Bytes no_size = text("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n");
  CHECK_FALSE(radiance_layout(no_size.data(), no_size.size()).has_value());
  const Bytes turned = text("#?RADIANCE\n\n+X 4 -Y 2\n");
  CHECK_FALSE(radiance_layout(turned.data(), turned.size()).has_value());
  const Bytes empty_side = text("#?RADIANCE\n\n-Y 0 +X 4\n");
  CHECK_FALSE(radiance_layout(empty_side.data(), empty_side.size()).has_value());
  const Bytes too_wide = header(kMaxImageSide + 1, 1);
  CHECK_FALSE(radiance_layout(too_wide.data(), too_wide.size()).has_value());
  const Bytes not_a_number = text("#?RADIANCE\n\n-Y 2x +X 4\n");
  CHECK_FALSE(radiance_layout(not_a_number.data(), not_a_number.size()).has_value());
  const Bytes good = flat_picture(kWide, kHigh, pixel);
  CHECK(radiance_layout(good.data(), good.size()).has_value());
}

TEST_CASE("pixels cut short or runs longer than their scanline are refused") {
  const Bytes pixel = {kFullMantissa, 0, 0, kOneExponent};
  Bytes cut = flat_picture(kWide, kHigh, pixel);
  cut.pop_back();
  CHECK_FALSE(decode_radiance(cut.data(), cut.size(), kNoLimit).has_value());

  Bytes overlong = header(kEncodableWidth, 1);
  overlong.insert(overlong.end(), {2, 2, 0, kEncodableWidth, kRepeat + kEncodableWidth + 1, kFullMantissa});
  CHECK_FALSE(decode_radiance(overlong.data(), overlong.size(), kNoLimit).has_value());

  Bytes empty_run = header(kEncodableWidth, 1);
  empty_run.insert(empty_run.end(), {2, 2, 0, kEncodableWidth, 0});
  CHECK_FALSE(decode_radiance(empty_run.data(), empty_run.size(), kNoLimit).has_value());

  Bytes literal_past_end = header(kEncodableWidth, 1);
  literal_past_end.insert(literal_past_end.end(), {2, 2, 0, kEncodableWidth, kEncodableWidth, 1, 2});
  CHECK_FALSE(decode_radiance(literal_past_end.data(), literal_past_end.size(), kNoLimit).has_value());
}
