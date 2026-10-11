#include "scene/gltf_checks.hpp"

#include <cstring>
#include <limits>
#include <optional>

#include "core/safe_path.hpp"

namespace redgem::gltf_checks {
namespace {

constexpr const char* kDataUriPrefix = "data:";  // data written into the file's own text
constexpr const char* kAddressMark = "://";      // what a web address has and a file name has not
constexpr std::size_t kLargestSize = std::numeric_limits<std::size_t>::max();

// a * b and a + b, when they do not wrap around.
std::optional<std::size_t> product(std::size_t a, std::size_t b) {
  if (b != 0 && a > kLargestSize / b) return std::nullopt;
  return a * b;
}

std::optional<std::size_t> sum(std::size_t a, std::size_t b) {
  if (a > kLargestSize - b) return std::nullopt;
  return a + b;
}

// Whether a buffer view lies within its buffer.
bool view_fits(const cgltf_buffer_view* view) {
  if (view == nullptr || view->buffer == nullptr) return false;
  return view->offset <= view->buffer->size && view->size <= view->buffer->size - view->offset;
}

// Whether the elements an accessor reads from its own buffer view are all inside it.
bool base_fits(const cgltf_accessor& accessor) {
  const std::size_t element = cgltf_calc_size(accessor.type, accessor.component_type);
  return view_fits(accessor.buffer_view) &&
         elements_fit(accessor.offset, accessor.stride, accessor.count, element, accessor.buffer_view->size);
}

// Whether the replacement values of a sparse accessor, and the list of which elements they
// replace, are all inside their buffer views.
bool sparse_fits(const cgltf_accessor& accessor) {
  const cgltf_accessor_sparse& sparse = accessor.sparse;
  if (sparse.count > accessor.count || !view_fits(sparse.indices_buffer_view) ||
      !view_fits(sparse.values_buffer_view)) {
    return false;
  }
  const std::size_t index = cgltf_component_size(sparse.indices_component_type);
  const std::size_t element = cgltf_calc_size(accessor.type, accessor.component_type);
  return elements_fit(sparse.indices_byte_offset, index, sparse.count, index, sparse.indices_buffer_view->size) &&
         elements_fit(sparse.values_byte_offset, accessor.stride, sparse.count, element,
                      sparse.values_buffer_view->size);
}

// Whether the file a buffer names may be read whole.
bool loadable_file(const cgltf_buffer& buffer, const std::filesystem::path& base_dir) {
  const std::filesystem::path file = file_of_uri(buffer.uri, base_dir);
  if (file.empty() || buffer.size > kMaxBufferFileBytes) return false;
  std::error_code fs_error;
  return std::filesystem::file_size(file, fs_error) <= kMaxBufferFileBytes && !fs_error;
}

std::string numbered(const char* what, std::size_t index) { return std::string(what) + " " + std::to_string(index); }

}  // namespace

bool elements_fit(std::size_t offset, std::size_t stride, std::size_t count, std::size_t element_size,
                  std::size_t size) {
  if (count == 0) return offset <= size;
  const std::optional<std::size_t> before_last = product(stride, count - 1);
  if (!before_last) return false;
  const std::optional<std::size_t> length = sum(*before_last, element_size);
  if (!length) return false;
  const std::optional<std::size_t> end = sum(offset, *length);
  return end && *end <= size;
}

std::string accessor_problem(const cgltf_accessor& accessor) {
  if (accessor.count > kMaxAccessorElements) {
    return "holds more than " + std::to_string(kMaxAccessorElements) + " elements";
  }
  if (accessor.buffer_view != nullptr && !base_fits(accessor)) return "reaches past its buffer view";
  if (accessor.is_sparse != 0 && !sparse_fits(accessor)) return "has sparse data that reaches past its buffer views";
  return {};
}

bool readable(const cgltf_accessor* accessor) {
  if (accessor == nullptr || accessor->is_sparse != 0 || accessor->buffer_view == nullptr) return false;
  return accessor_problem(*accessor).empty() && cgltf_buffer_view_data(accessor->buffer_view) != nullptr;
}

std::string data_problem(const cgltf_data& data) {
  if (data.nodes_count > kMaxSceneNodes) return "it holds more than " + std::to_string(kMaxSceneNodes) + " nodes";
  for (cgltf_size i = 0; i < data.buffer_views_count; ++i) {
    if (!view_fits(&data.buffer_views[i])) return numbered("buffer view", i) + " reaches past its buffer";
  }
  for (cgltf_size i = 0; i < data.accessors_count; ++i) {
    const std::string problem = accessor_problem(data.accessors[i]);
    if (!problem.empty()) return numbered("accessor", i) + " " + problem;
  }
  return {};
}

std::string buffer_file_problem(const cgltf_data& data, const std::filesystem::path& base_dir) {
  for (cgltf_size i = 0; i < data.buffers_count; ++i) {
    if (!names_a_file(data.buffers[i].uri) || loadable_file(data.buffers[i], base_dir)) continue;
    return numbered("buffer", i) + " names a file that is not a readable file of the scene's own folder";
  }
  return {};
}

bool names_a_file(const char* uri) {
  if (uri == nullptr) return false;
  return std::strncmp(uri, kDataUriPrefix, std::strlen(kDataUriPrefix)) != 0 &&
         std::strstr(uri, kAddressMark) == nullptr;
}

std::filesystem::path file_of_uri(const char* uri, const std::filesystem::path& base_dir) {
  if (!names_a_file(uri)) return {};
  std::string name(uri);
  name.resize(cgltf_decode_uri(name.data()));
  // (An escaped end-of-text byte would make the name mean one thing here and another to the system.)
  if (name.find('\0') != std::string::npos) return {};
  return file_inside(base_dir, name).value_or(std::filesystem::path());
}

}  // namespace redgem::gltf_checks
