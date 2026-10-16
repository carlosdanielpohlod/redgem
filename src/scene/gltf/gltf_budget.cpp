#include "scene/gltf/gltf_budget.hpp"

#include <algorithm>
#include <limits>

#include "core/image_header.hpp"
#include "scene/gltf/gltf_checks.hpp"

namespace redgem::gltf_budget {
namespace {

constexpr std::uint64_t kMost = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint64_t kBytesPerComponent = 4;  // a float or a 32-bit index, as the engine keeps them
constexpr std::uint64_t kBytesPerMegabyte = 1024 * 1024;
constexpr std::uint64_t kBytesPerPixel = 4;  // red, green, blue and alpha, as pictures are decoded

// a + b and a * b, staying at the largest number instead of wrapping around.
std::uint64_t plus(std::uint64_t a, std::uint64_t b) { return a > kMost - b ? kMost : a + b; }

std::uint64_t times(std::uint64_t a, std::uint64_t b) { return b != 0 && a > kMost / b ? kMost : a * b; }

std::uint64_t size_of(const std::filesystem::path& file) {
  std::error_code fs_error;
  const std::uintmax_t size = std::filesystem::file_size(file, fs_error);
  return fs_error ? 0 : size;
}

// What the buffers kept apart from the file's own bytes hold: data files, and data written into
// the file's text (which is decoded into a copy of its own).
std::uint64_t buffer_bytes(const cgltf_data& data) {
  std::uint64_t bytes = 0;
  for (cgltf_size i = 0; i < data.buffers_count; ++i) {
    if (data.buffers[i].uri != nullptr) bytes = plus(bytes, data.buffers[i].size);
  }
  return bytes;
}

// Pictures in files of their own are read whole; the ones in the scene's data are copied out of it.
std::uint64_t image_bytes(const cgltf_data& data, const std::filesystem::path& folder) {
  std::uint64_t bytes = 0;
  for (cgltf_size i = 0; i < data.images_count; ++i) {
    const cgltf_image& image = data.images[i];
    if (image.buffer_view != nullptr) bytes = plus(bytes, image.buffer_view->size);
    if (gltf_checks::names_a_file(image.uri)) bytes = plus(bytes, size_of(gltf_checks::file_of_uri(image.uri, folder)));
  }
  return bytes;
}

// (An accessor that claims more elements than one may hold is refused by itself: see gltf_checks.)
std::uint64_t accessor_bytes(const cgltf_data& data) {
  std::uint64_t bytes = 0;
  for (cgltf_size i = 0; i < data.accessors_count; ++i) {
    const cgltf_accessor& accessor = data.accessors[i];
    const std::uint64_t elements = std::min<std::uint64_t>(accessor.count, gltf_checks::kMaxAccessorElements);
    bytes = plus(bytes, times(elements, cgltf_num_components(accessor.type) * kBytesPerComponent));
  }
  return bytes;
}

std::string too_much(std::uint64_t bytes, const char* what) {
  return what + std::to_string(bytes / kBytesPerMegabyte) + " MB of memory, more than the " +
         std::to_string(kMaxSceneBytes / kBytesPerMegabyte) + " MB a scene may";
}

}  // namespace

std::string file_problem(const std::filesystem::path& file) { return file_size_problem(size_of(file)); }

std::string file_size_problem(std::uint64_t bytes) {
  if (bytes <= kMaxSceneBytes) return {};
  return too_much(bytes, "the file alone takes ");
}

std::uint64_t scene_bytes(const cgltf_data& data, const std::filesystem::path& file) {
  const std::uint64_t data_bytes = plus(buffer_bytes(data), image_bytes(data, file.parent_path()));
  return plus(plus(size_of(file), data_bytes), accessor_bytes(data));
}

std::uint64_t decoded_bytes(const std::vector<ImageData>& images) {
  std::uint64_t bytes = 0;
  for (const ImageData& image : images) {
    if (!decodable_image(image.encoded.data(), image.encoded.size())) continue;
    const ImageSize size = *encoded_image_size(image.encoded.data(), image.encoded.size());
    bytes = plus(bytes, std::uint64_t{size.width} * size.height * kBytesPerPixel);
  }
  return bytes;
}

std::string pictures_problem(const cgltf_data& data, const std::filesystem::path& file,
                             const std::vector<ImageData>& images) {
  const std::uint64_t bytes = plus(scene_bytes(data, file), decoded_bytes(images));
  if (bytes <= kMaxSceneBytes) return {};
  return too_much(bytes, "with its pictures decoded it would take about ");
}

std::string scene_problem(const cgltf_data& data, const std::filesystem::path& file) {
  const std::uint64_t bytes = scene_bytes(data, file);
  if (bytes <= kMaxSceneBytes) return {};
  return too_much(bytes, "loading it would take about ");
}

}  // namespace redgem::gltf_budget
