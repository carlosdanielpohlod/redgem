#include "core/ktx2_image.hpp"

// (<exception> first: the transcoder's header uses std::terminate without including it.)
#include <exception>
#include <basisu_transcoder.h>

#include <iterator>
#include <memory>
#include <mutex>
#include <utility>

#include "core/ktx2_header.hpp"

namespace redgem {
namespace {

using Transcoder = basist::ktx2_transcoder;
using basist::transcoder_texture_format;

constexpr std::uint32_t kOnlyLayer = 0;
constexpr std::uint32_t kOnlyFace = 0;
constexpr std::uint32_t kFullSize = 0;  // the level of the full-size picture
constexpr std::size_t kRgbaBytes = 4;

// The transcoder's tables are filled once, before its first picture.
void prepare_transcoder() {
  static std::once_flag prepared;
  std::call_once(prepared, [] { basist::basisu_transcoder_init(); });
}

// What the transcoder calls each format, in GpuBlockFormat's order.
constexpr transcoder_texture_format kTranscoderFormats[] = {
    transcoder_texture_format::cTFBC1_RGB,       transcoder_texture_format::cTFBC3_RGBA,
    transcoder_texture_format::cTFASTC_4x4_RGBA, transcoder_texture_format::cTFETC1_RGB,
    transcoder_texture_format::cTFETC2_RGBA,     transcoder_texture_format::cTFRGBA32};
static_assert(std::size(kTranscoderFormats) == static_cast<std::size_t>(GpuBlockFormat::Rgba8) + 1,
              "kTranscoderFormats follows GpuBlockFormat");

transcoder_texture_format transcoder_format(GpuBlockFormat format) {
  return kTranscoderFormats[static_cast<std::size_t>(format)];
}

// The two kinds of Basis data KHR_texture_basisu allows: ETC1S and UASTC LDR 4x4.
bool khr_basisu_data(const Transcoder& transcoder) { return transcoder.is_etc1s() || transcoder.is_uastc(); }

// A transcoder ready for the file's levels; null when the file is refused.
std::unique_ptr<Transcoder> opened_transcoder(const std::uint8_t* data, std::size_t size) {
  if (!read_ktx2_header(data, size)) return nullptr;
  prepare_transcoder();
  auto transcoder = std::make_unique<Transcoder>();
  if (!transcoder->init(data, static_cast<std::uint32_t>(size))) return nullptr;
  if (!khr_basisu_data(*transcoder)) return nullptr;
  if (!transcoder->start_transcoding()) return nullptr;
  return transcoder;
}

// How many blocks (or, for a format that is not compressed, pixels) a level is made of.
std::uint32_t units_of(const basist::ktx2_image_level_info& info, transcoder_texture_format format) {
  if (basist::basis_transcoder_format_is_uncompressed(format)) return info.m_orig_width * info.m_orig_height;
  return info.m_total_blocks;
}

std::optional<TranscodedLevel> transcode_level(Transcoder& transcoder, std::uint32_t level, GpuBlockFormat format) {
  basist::ktx2_image_level_info info{};
  if (!transcoder.get_image_level_info(info, level, kOnlyLayer, kOnlyFace)) return std::nullopt;
  const transcoder_texture_format target = transcoder_format(format);
  const std::uint32_t units = units_of(info, target);
  TranscodedLevel out{
      ImageSize{info.m_orig_width, info.m_orig_height},
      std::vector<std::uint8_t>(std::size_t{units} * basist::basis_get_bytes_per_block_or_pixel(target))};
  if (!transcoder.transcode_image_level(level, kOnlyLayer, kOnlyFace, out.bytes.data(), units, target))
    return std::nullopt;
  return out;
}

// The full-size level as RGBA8 into `output`; false when the transcoder refuses it, or `capacity`
// falls short of it.
bool decode_full_size_into(Transcoder& transcoder, std::uint8_t* output, std::size_t capacity) {
  basist::ktx2_image_level_info info{};
  if (!transcoder.get_image_level_info(info, kFullSize, kOnlyLayer, kOnlyFace)) return false;
  const std::uint32_t pixels = info.m_orig_width * info.m_orig_height;
  if (capacity < std::size_t{pixels} * kRgbaBytes) return false;
  return transcoder.transcode_image_level(kFullSize, kOnlyLayer, kOnlyFace, output, pixels,
                                          transcoder_texture_format::cTFRGBA32);
}

}  // namespace

std::optional<TranscodedPicture> transcode_ktx2(const std::uint8_t* data, std::size_t size,
                                                const GpuTextureSupport& support) {
  const std::unique_ptr<Transcoder> transcoder = opened_transcoder(data, size);
  if (!transcoder) return std::nullopt;
  TranscodedPicture picture;
  picture.has_alpha = transcoder->get_has_alpha() != 0;
  picture.format = best_block_format(support, picture.has_alpha);
  const auto levels = static_cast<std::uint32_t>(transcoder->get_level_index().size());
  for (std::uint32_t level = 0; level < levels; ++level) {
    std::optional<TranscodedLevel> transcoded = transcode_level(*transcoder, level, picture.format);
    if (!transcoded) return std::nullopt;
    picture.levels.push_back(std::move(*transcoded));
  }
  return picture;
}

bool opens_ktx2(const std::uint8_t* data, std::size_t size) { return opened_transcoder(data, size) != nullptr; }

bool decode_ktx2_rgba_into(const std::uint8_t* data, std::size_t size, std::uint8_t* output, std::size_t capacity) {
  if (output == nullptr) return false;
  const std::unique_ptr<Transcoder> transcoder = opened_transcoder(data, size);
  return transcoder && decode_full_size_into(*transcoder, output, capacity);
}

std::optional<TranscodedLevel> decode_ktx2_rgba(const std::uint8_t* data, std::size_t size) {
  const std::optional<Ktx2Header> header = read_ktx2_header(data, size);
  if (!header) return std::nullopt;
  TranscodedLevel level{header->size,
                        std::vector<std::uint8_t>(std::size_t{header->size.width} * header->size.height * kRgbaBytes)};
  if (!decode_ktx2_rgba_into(data, size, level.bytes.data(), level.bytes.size())) return std::nullopt;
  return level;
}

}  // namespace redgem
