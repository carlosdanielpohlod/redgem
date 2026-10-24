// A game's icon, scaled to the sizes systems want, with the image functions the engine draws with.

#include "project/icons/icon_set.hpp"

#include <raylib.h>

#include "core/image_header.hpp"
#include "core/safe_path.hpp"
#include "core/translations.hpp"

namespace redgem {
namespace {

constexpr const char* kPngFileType = ".png";  // what raylib is told the bytes are, and writes

// A decoded picture, given back when it goes.
struct Picture {
  Image image{};
  Picture() = default;
  explicit Picture(Image decoded) : image(decoded) {}
  Picture(const Picture&) = delete;
  Picture& operator=(const Picture&) = delete;
  ~Picture() {
    if (image.data != nullptr) UnloadImage(image);
  }
};

// Whether a file is a picture an icon can be made of: one that can be decoded, as wide as it is high.
bool square_picture(const bytes::Bytes& png) {
  const auto size = encoded_image_size(png.data(), png.size());
  return size && size->width == size->height && decodable_image(png.data(), png.size());
}

// The picture of a PNG file, as 8-bit RGBA; no picture for anything else, or one that is not square.
bool decode(const std::optional<bytes::Bytes>& png, Picture& picture) {
  if (!png || !square_picture(*png)) return false;
  picture.image = LoadImageFromMemory(kPngFileType, png->data(), static_cast<int>(png->size()));
  if (picture.image.data == nullptr) return false;
  ImageFormat(&picture.image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  return true;
}

// The picture scaled to `side`, as a PNG; empty when it cannot be written.
bytes::Bytes scaled_png(const Image& image, int side) {
  Picture scaled(ImageCopy(image));
  ImageResize(&scaled.image, side, side);
  int size = 0;
  unsigned char* written = ExportImageToMemory(scaled.image, kPngFileType, &size);
  bytes::Bytes png(written, written + (written != nullptr ? size : 0));
  MemFree(written);
  return png;
}

std::string unusable(const std::filesystem::path& png) {
  return tr("project.icon.unusable", {{"picture", printable(png.generic_string())}});
}

}  // namespace

std::optional<IconSet> scaled_icons(const std::filesystem::path& png, const int* sides, std::size_t count,
                                    std::string& error) {
  SetTraceLogLevel(LOG_WARNING);  // (raylib tells of every picture it decodes: a build says what goes wrong)
  Picture picture;
  if (!decode(read_file_up_to(png, kMaxEncodedImageBytes), picture)) {
    error = unusable(png);
    return std::nullopt;
  }
  IconSet icons;
  for (std::size_t i = 0; i < count; ++i) icons[sides[i]] = scaled_png(picture.image, sides[i]);
  return icons;
}

}  // namespace redgem
