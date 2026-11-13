// KTX2 pictures of Basis Universal data (glTF's KHR_texture_basisu) as they could arrive damaged
// or made up: the header and level index read and checked, then every level transcoded to each
// format a graphics card may take, and the full-size picture decoded to RGBA8.
#include <cstdint>
#include <optional>

#include "core/gpu_texture_formats.hpp"
#include "core/ktx2_header.hpp"
#include "core/ktx2_image.hpp"
#include "fuzz/fuzz_target.hpp"

namespace {

// The most pixels a picture may have to be transcoded here: enough for any made-up input that
// gets through its header, small enough that each run stays quick.
constexpr std::uint64_t kMostPixels = 256 * 256;

// One card of each kind: BCn, ASTC, ETC2 and none (RGBA8).
constexpr redgem::GpuTextureSupport kCards[] = {{true, false, false}, {false, true, false}, {false, false, true}, {}};

// A picture whose header passed, and small enough to transcode here.
bool worth_transcoding(const std::optional<redgem::Ktx2Header>& header) {
  return header && static_cast<std::uint64_t>(header->size.width) * header->size.height <= kMostPixels;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  redgem::starts_as_ktx2(data, size);
  if (!worth_transcoding(redgem::read_ktx2_header(data, size))) return redgem::fuzz::kKeepInput;
  for (const redgem::GpuTextureSupport& card : kCards) redgem::transcode_ktx2(data, size, card);
  redgem::decode_ktx2_rgba(data, size);
  return redgem::fuzz::kKeepInput;
}
