#include "editor/document/picture_fit.hpp"

#include <raylib.h>

#include "core/image_header.hpp"
#include "core/import_settings.hpp"
#include "render/texture_loading.hpp"

namespace redgem::picture_fit {
namespace {

constexpr const char* kPngKind = ".png";

using import_settings::PictureSize;

PictureSize size_of(const EncodedImage& image) {
  const ImageSize size = image.dimensions();
  return {static_cast<int>(size.width), static_cast<int>(size.height)};
}

// The decoded picture written as PNG; nullopt when it cannot be.
std::optional<std::vector<std::uint8_t>> png_of(const Image& decoded) {
  int bytes = 0;
  unsigned char* written = ExportImageToMemory(decoded, kPngKind, &bytes);
  if (written == nullptr) return std::nullopt;
  std::vector<std::uint8_t> png(written, written + bytes);
  MemFree(written);
  return png;
}

}  // namespace

std::optional<std::vector<std::uint8_t>> fitted_picture(const std::vector<std::uint8_t>& encoded, int max_size) {
  const auto image = EncodedImage::read(encoded.data(), encoded.size());
  if (!image) return std::nullopt;
  const PictureSize own = size_of(*image);
  if (import_settings::fitted_size(own, max_size) == own) return encoded;
  Image decoded = render_detail::decode_picture(encoded);
  if (!IsImageValid(decoded)) return std::nullopt;
  render_detail::fit_picture(decoded, max_size);
  auto png = png_of(decoded);
  UnloadImage(decoded);
  return png;
}

}  // namespace redgem::picture_fit
