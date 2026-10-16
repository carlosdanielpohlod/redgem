#include "scene/gltf/gltf_checks.hpp"

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
  const bool wraps = b != 0 && a > kLargestSize / b;
  if (wraps) return std::nullopt;
  return a * b;
}

std::optional<std::size_t> sum(std::size_t a, std::size_t b) {
  if (a > kLargestSize - b) return std::nullopt;
  return a + b;
}

bool has_buffer(const cgltf_buffer_view* view) { return view != nullptr && view->buffer != nullptr; }

// Whether a buffer view lies within its buffer.
bool view_fits(const cgltf_buffer_view* view) {
  if (!has_buffer(view)) return false;
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
  const bool placed =
      sparse.count <= accessor.count && view_fits(sparse.indices_buffer_view) && view_fits(sparse.values_buffer_view);
  if (!placed) return false;
  const std::size_t index = cgltf_component_size(sparse.indices_component_type);
  const std::size_t element = cgltf_calc_size(accessor.type, accessor.component_type);
  return elements_fit(sparse.indices_byte_offset, index, sparse.count, index, sparse.indices_buffer_view->size) &&
         elements_fit(sparse.values_byte_offset, accessor.stride, sparse.count, element,
                      sparse.values_buffer_view->size);
}

// Whether the file a buffer names (found already) may be read whole.
bool loadable_file(const cgltf_buffer& buffer, const std::filesystem::path& file) {
  const bool may_be_opened = !file.empty() && buffer.size <= kMaxBufferFileBytes;
  if (!may_be_opened) return false;
  std::error_code fs_error;
  return std::filesystem::file_size(file, fs_error) <= kMaxBufferFileBytes && !fs_error;
}

// (A buffer that names no file keeps its data in the scene's own file.)
bool may_be_loaded(const cgltf_buffer& buffer, const std::filesystem::path& file) {
  return !names_a_file(buffer.uri) || loadable_file(buffer, file);
}

template <typename Entry>
std::vector<std::filesystem::path> files_of(const Entry* entries, cgltf_size count,
                                            const std::filesystem::path& base_dir) {
  std::vector<std::filesystem::path> files;
  files.reserve(count);
  for (cgltf_size i = 0; i < count; ++i) files.push_back(file_of_uri(entries[i].uri, base_dir));
  return files;
}

bool base_reaches_past(const cgltf_accessor& accessor) {
  return accessor.buffer_view != nullptr && !base_fits(accessor);
}

bool sparse_reaches_past(const cgltf_accessor& accessor) { return accessor.is_sparse != 0 && !sparse_fits(accessor); }

// Sparse accessors and those without a buffer view are treated as absent.
bool read_as_it_is(const cgltf_accessor* accessor) {
  return accessor != nullptr && accessor->is_sparse == 0 && accessor->buffer_view != nullptr;
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
  if (base_reaches_past(accessor)) return "reaches past its buffer view";
  if (sparse_reaches_past(accessor)) return "has sparse data that reaches past its buffer views";
  return {};
}

bool readable(const cgltf_accessor* accessor) {
  if (!read_as_it_is(accessor)) return false;
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

NamedFiles named_files(const cgltf_data& data, const std::filesystem::path& base_dir) {
  return {files_of(data.buffers, data.buffers_count, base_dir), files_of(data.images, data.images_count, base_dir)};
}

std::string buffer_file_problem(const cgltf_data& data, const NamedFiles& files) {
  for (cgltf_size i = 0; i < data.buffers_count; ++i) {
    if (may_be_loaded(data.buffers[i], files.buffers[i])) continue;
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
