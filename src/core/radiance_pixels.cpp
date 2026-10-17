// A Radiance picture's pixels: four bytes each (red, green, blue mantissas and a shared exponent),
// a scanline at a time, flat or run-length encoded channel by channel.

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/radiance_image.hpp"

namespace redgem {
namespace {

constexpr std::size_t kBytesPerPixel = 4;
constexpr std::size_t kExponent = 3;           // the byte of a pixel that scales the other three
constexpr std::uint32_t kShortestEncoded = 8;  // scanlines outside these widths are always flat
constexpr std::uint32_t kLongestEncoded = 0x7FFF;
constexpr std::uint8_t kEncodedMark = 2;  // an encoded scanline starts 2, 2, then its width
constexpr std::size_t kWidthHigh = 2;     // the bytes of that width
constexpr std::size_t kWidthLow = 3;
constexpr std::size_t kRed = 0;
constexpr std::size_t kGreen = 1;
constexpr std::size_t kBlue = 2;
constexpr std::uint8_t kRepeatAbove = 128;  // a count above this repeats one byte (count - 128) times
constexpr int kExponentBias = 136;          // 128, and 8 more for the mantissa's bits
constexpr int kBitsPerByte = 8;

// Bytes read one after the other, never past the end.
struct Cursor {
  const std::uint8_t* data;
  std::size_t size;
  std::size_t at;

  bool has(std::size_t count) const { return size - at >= count; }
  std::uint8_t take() { return data[at++]; }
};

bool encodable_width(std::uint32_t width) { return width >= kShortestEncoded && width <= kLongestEncoded; }

// Whether the scanline at the cursor is run-length encoded: it starts 2, 2 and its own width.
bool encoded_scanline(const Cursor& in, std::uint32_t width) {
  if (!encodable_width(width) || !in.has(kBytesPerPixel)) return false;
  const std::uint8_t* start = in.data + in.at;
  return start[0] == kEncodedMark && start[1] == kEncodedMark &&
         ((static_cast<std::uint32_t>(start[kWidthHigh]) << kBitsPerByte) | start[kWidthLow]) == width;
}

// Whether a run of `pixels` fits in the scanline from pixel `x` on, and its bytes (`bytes`) in the file.
bool run_fits(const Cursor& in, std::size_t row_pixels, std::uint32_t x, std::uint32_t pixels, std::size_t bytes) {
  return pixels > 0 && pixels <= row_pixels - x && in.has(bytes);
}

// One run of a channel from pixel `x` on: how many pixels it filled, 0 when it is malformed.
std::uint32_t read_run(Cursor& in, std::vector<std::uint8_t>& row, std::uint32_t x, std::size_t channel) {
  if (!in.has(1)) return 0;
  const std::uint8_t count = in.take();
  const bool repeated = count > kRepeatAbove;
  const std::uint32_t pixels = repeated ? count - kRepeatAbove : count;
  if (!run_fits(in, row.size() / kBytesPerPixel, x, pixels, repeated ? 1 : pixels)) return 0;
  const std::uint8_t repeat = repeated ? in.take() : 0;
  for (std::uint32_t i = 0; i < pixels; ++i) row[(x + i) * kBytesPerPixel + channel] = repeated ? repeat : in.take();
  return pixels;
}

bool read_encoded(Cursor& in, std::vector<std::uint8_t>& row, std::uint32_t width) {
  in.at += kBytesPerPixel;
  for (std::size_t channel = 0; channel < kBytesPerPixel; ++channel) {
    for (std::uint32_t x = 0; x < width;) {
      const std::uint32_t filled = read_run(in, row, x, channel);
      if (filled == 0) return false;
      x += filled;
    }
  }
  return true;
}

bool read_flat(Cursor& in, std::vector<std::uint8_t>& row) {
  if (!in.has(row.size())) return false;
  std::copy(in.data + in.at, in.data + in.at + row.size(), row.begin());
  in.at += row.size();
  return true;
}

Vec3 light_of(const std::uint8_t* pixel) {
  if (pixel[kExponent] == 0) return Vec3::zero();
  const float scale = std::ldexp(1.0f, static_cast<int>(pixel[kExponent]) - kExponentBias);
  return Vec3{static_cast<float>(pixel[kRed]), static_cast<float>(pixel[kGreen]), static_cast<float>(pixel[kBlue])} *
         scale;
}

// Adds a decoded scanline (row `y` of the file) to the shrunk picture.
void add_row(FloatImage& image, const std::vector<std::uint8_t>& row, std::uint32_t y, std::uint32_t shrink) {
  const float share = 1.0f / static_cast<float>(shrink * shrink);
  for (std::uint32_t x = 0; x < image.width * shrink; ++x) {
    add_share(image, x / shrink, y / shrink, light_of(&row[x * kBytesPerPixel]), share);
  }
}

bool read_scanline(Cursor& in, std::vector<std::uint8_t>& row, std::uint32_t width) {
  return encoded_scanline(in, width) ? read_encoded(in, row, width) : read_flat(in, row);
}

std::optional<FloatImage> read_pixels(Cursor in, const RadianceLayout& layout, std::uint32_t shrink) {
  FloatImage image = black_image(layout.width / shrink, layout.height / shrink);
  std::vector<std::uint8_t> row(static_cast<std::size_t>(layout.width) * kBytesPerPixel);
  for (std::uint32_t y = 0; y < image.height * shrink; ++y) {
    if (!read_scanline(in, row, layout.width)) return std::nullopt;
    add_row(image, row, y, shrink);
  }
  return image;
}

}  // namespace

std::optional<FloatImage> decode_radiance(const std::uint8_t* data, std::size_t size, std::uint32_t widest) {
  const std::optional<RadianceLayout> layout = radiance_layout(data, size);
  if (!layout) return std::nullopt;
  const std::uint32_t shrink = shrink_to_fit(layout->width, std::max(widest, 1u));
  if (layout->height < shrink) return std::nullopt;  // too thin to shrink: not a sky
  return read_pixels(Cursor{data, size, layout->pixels_at}, *layout, shrink);
}

}  // namespace redgem
