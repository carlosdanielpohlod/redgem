#include "editor/editor_app.hpp"

#include <algorithm>
#include <memory>

#include <raylib.h>

#include "editor/scene_viewport.hpp"

namespace redgem {
namespace {
constexpr std::size_t kMaxImageBytes = 128 * 1024;
constexpr int kSmallestCapture = 128;
constexpr int kLargestCapture = 1024;
constexpr int kShrinkFactor = 2;
constexpr const char* kCaptureFormat = ".png";

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

EditorApp::ViewportPicture encode(Image& image) {
  auto bytes = png(image);
  while (bytes.size() > kMaxImageBytes && std::max(image.width, image.height) > kSmallestCapture) {
    fit(image, std::max(kSmallestCapture, std::max(image.width, image.height) / kShrinkFactor));
    bytes = png(image);
  }
  if (bytes.size() > kMaxImageBytes) return {};
  return {std::move(bytes), image.width, image.height};
}
std::optional<EditorApp::ViewportPicture> failed_picture(std::string& error, const char* message) {
  error = message;
  return std::nullopt;
}
}  // namespace

std::optional<EditorApp::ViewportPicture> EditorApp::capture_viewport(int max_side, std::string& error) const {
  if (max_side < kSmallestCapture || max_side > kLargestCapture)
    return failed_picture(error, "capture size must be between 128 and 1024");
  HeldImage picture{LoadImageFromTexture(viewport_->target.texture)};
  if (!picture.image.data) return failed_picture(error, "viewport is not ready");
  ImageFlipVertical(&picture.image);
  fit(picture.image, max_side);
  auto result = encode(picture.image);
  if (result.bytes.empty()) return failed_picture(error, "could not encode the viewport");
  return result;
}
}  // namespace redgem
