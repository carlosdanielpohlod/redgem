#pragma once

// How big a picture is, read from the first bytes of its file without decoding it. A picture's
// header is a claim about how much memory decoding it will take: it is checked before any is.

#include <cstddef>
#include <cstdint>
#include <optional>

namespace redgem {

// The most bytes of an encoded picture that are decoded, and the most pixels along each of its
// sides (larger than any graphics card of the machines the engine runs on takes as one texture).
inline constexpr std::size_t kMaxEncodedImageBytes = 256u * 1024u * 1024u;
inline constexpr std::uint32_t kMaxImageSide = 8192;

struct ImageSize {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

enum class EncodedImageFormat { Png, Jpeg, Webp };

// A borrowed image whose encoded length, static format and dimensions were checked once.
// Keep its source bytes alive and unchanged until decoding finishes.
class EncodedImage {
 public:
  static std::optional<EncodedImage> read(const std::uint8_t* data, std::size_t bytes);
  ImageSize dimensions() const { return size_; }
  EncodedImageFormat format() const { return format_; }
  const std::uint8_t* data() const { return data_; }
  std::size_t byte_count() const { return bytes_; }

 private:
  EncodedImage(const std::uint8_t* data, std::size_t bytes, ImageSize size, EncodedImageFormat format)
      : data_(data), bytes_(bytes), size_(size), format_(format) {}
  const std::uint8_t* data_;
  std::size_t bytes_;
  ImageSize size_;
  EncodedImageFormat format_;
};

// The size a PNG, JPEG or static WebP file says its picture has; nullopt for unsupported
// formats, animation or a header that is cut short.
std::optional<ImageSize> encoded_image_size(const std::uint8_t* data, std::size_t size);

// Whether an encoded picture may be decoded: PNG, JPEG or static WebP within the limits above, with at least
// one pixel.
bool decodable_image(const std::uint8_t* data, std::size_t size);

}  // namespace redgem
