#include "editor/editor_app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

#include <raylib.h>

#include "editor/scene_viewport.hpp"

namespace redgem {
namespace {
constexpr int kSmallestCapture = 128;
constexpr int kLargestCapture = 1024;
// A picture too big is made smaller by a quarter at a time: as large as fits, not half of it.
constexpr int kShrinkNumerator = 3;
constexpr int kShrinkDenominator = 4;
constexpr const char* kCaptureFormat = ".png";
constexpr double kEstimateMargin = 0.95;  // the side estimated to fit is taken this much smaller

struct HeldImage {
  Image image;
  ~HeldImage() { UnloadImage(image); }
};

void fit(Image& image, int side) {
  const int largest = std::max(image.width, image.height);
  if (largest <= side) return;
  ImageResize(&image, std::max(1, image.width * side / largest), std::max(1, image.height * side / largest));
}

std::vector<std::uint8_t> png(const Image& image) {
  int size = 0;
  std::unique_ptr<unsigned char, void (*)(void*)> bytes(ExportImageToMemory(image, kCaptureFormat, &size), MemFree);
  if (!bytes || size <= 0) return {};
  return std::vector<std::uint8_t>(bytes.get(), bytes.get() + size);
}

bool fits(const std::vector<std::uint8_t>& bytes, std::size_t max_bytes) {
  return !bytes.empty() && bytes.size() <= max_bytes;
}

// A side to try next for a picture of `bytes` at `side` that must fit `max_bytes`: a PNG grows about
// with its area, so the side that would fit, a little under; and at least a quarter smaller.
int smaller_side(int side, std::size_t bytes, std::size_t max_bytes) {
  const double fitting =
      side * std::sqrt(static_cast<double>(max_bytes) / static_cast<double>(bytes)) * kEstimateMargin;
  return std::max(kSmallestCapture, std::min(static_cast<int>(fitting), side * kShrinkNumerator / kShrinkDenominator));
}

// The picture at the largest side up to `side` whose PNG fits `max_bytes`, each try made from the
// original (never a picture already made smaller). Empty when not even the smallest fits.
EditorApp::ViewportPicture encode(const Image& original, int side, std::size_t max_bytes) {
  for (;;) {
    HeldImage candidate{ImageCopy(original)};
    fit(candidate.image, side);
    auto bytes = png(candidate.image);
    if (fits(bytes, max_bytes)) return {std::move(bytes), candidate.image.width, candidate.image.height};
    if (side <= kSmallestCapture || bytes.empty()) return {};
    side = smaller_side(side, bytes.size(), max_bytes);
  }
}

std::optional<EditorApp::ViewportPicture> failed_picture(std::string& error, const char* message) {
  error = message;
  return std::nullopt;
}
}  // namespace

std::optional<EditorApp::ViewportPicture> EditorApp::capture_viewport(int max_side, std::size_t max_bytes,
                                                                      std::string& error) const {
  if (max_side < kSmallestCapture || max_side > kLargestCapture)
    return failed_picture(error, "capture size must be between 128 and 1024");
  HeldImage picture{LoadImageFromTexture(viewport_->target.texture)};
  if (!picture.image.data) return failed_picture(error, "viewport is not ready");
  ImageFlipVertical(&picture.image);
  ImageFormat(&picture.image, PIXELFORMAT_UNCOMPRESSED_R8G8B8);  // (the viewport has no transparency to keep)
  auto result = encode(picture.image, max_side, max_bytes);
  if (result.bytes.empty()) return failed_picture(error, "could not encode the viewport");
  return result;
}
}  // namespace redgem
