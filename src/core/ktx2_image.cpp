#include "core/ktx2_image.hpp"

#include <basisu_transcoder.h>

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

std::optional<TranscodedLevel> decode_ktx2_rgba(const std::uint8_t* data, std::size_t size) {
  const std::unique_ptr<Transcoder> transcoder = opened_transcoder(data, size);
  if (!transcoder) return std::nullopt;
  return transcode_level(*transcoder, kFullSize, GpuBlockFormat::Rgba8);
}

}  // namespace redgem
