#include "scene/gltf/gltf_loader.hpp"

#include <cgltf.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <utility>

#include "core/game_files.hpp"
#include "scene/gltf/gltf_budget.hpp"
#include "scene/gltf/gltf_checks.hpp"
#include "scene/gltf/gltf_reader.hpp"

namespace redgem {
namespace {

struct CgltfDeleter {
  void operator()(cgltf_data* data) const { cgltf_free(data); }
};
using CgltfPtr = std::unique_ptr<cgltf_data, CgltfDeleter>;

// Why cgltf could not read a file, in words.
struct ResultName {
  cgltf_result result = cgltf_result_success;
  const char* name = nullptr;
};
constexpr ResultName kResultNames[] = {
    {cgltf_result_success, "success"},
    {cgltf_result_data_too_short, "data too short"},
    {cgltf_result_unknown_format, "unknown format"},
    {cgltf_result_invalid_json, "invalid json"},
    {cgltf_result_invalid_gltf, "invalid glTF"},
    {cgltf_result_invalid_options, "invalid options"},
    {cgltf_result_file_not_found, "file not found"},
    {cgltf_result_io_error, "io error"},
    {cgltf_result_out_of_memory, "out of memory"},
    {cgltf_result_legacy_gltf, "legacy glTF"},
};

std::string describe(cgltf_result result) {
  for (const ResultName& known : kResultNames) {
    if (known.result == result) return known.name;
  }
  return "unknown error";
}

// One step of opening a file: false (and why, in `error`) when it failed.
bool succeeded(cgltf_result status, const char* failure, const std::string& path, std::string& error) {
  if (status == cgltf_result_success) return true;
  error = std::string(failure) + " '" + path + "': " + describe(status);
  return false;
}

// One of the loader's own checks: false (and what is wrong, in `error`) when it found something.
bool sound(const std::string& problem, const std::string& path, std::string& error) {
  if (problem.empty()) return true;
  error = "invalid glTF '" + path + "': " + problem;
  return false;
}

std::filesystem::path folder_of(const std::string& path) { return std::filesystem::path(path).parent_path(); }

bool file_exceeds_budget(const std::string& path, std::uint64_t max_bytes) {
  return game_files::file_size(path).value_or(0) > max_bytes;
}

// How many bytes of a file cgltf asks for: `size` when it says, else all of it.
cgltf_size wanted_bytes(const cgltf_size* size, std::size_t whole) {
  return (size != nullptr && *size != 0) ? *size : whole;
}

// cgltf reads the scene's file and the data files it names through the engine's own reading, so a
// built game's scenes come out of its archive (see core/game_files.hpp).
cgltf_result read_game_file(const cgltf_memory_options* /*memory*/, const cgltf_file_options* /*file*/,
                            const char* path, cgltf_size* size, void** data) {
  const auto bytes = game_files::read(path, kMaxArchiveEntryBytes);
  if (!bytes) return cgltf_result_file_not_found;
  const cgltf_size wanted = wanted_bytes(size, bytes->size());
  if (wanted > bytes->size()) return cgltf_result_io_error;
  void* copy = std::malloc(wanted == 0 ? 1 : wanted);
  if (copy == nullptr) return cgltf_result_out_of_memory;
  if (wanted > 0) std::memcpy(copy, bytes->data(), wanted);
  if (size != nullptr) *size = wanted;
  *data = copy;
  return cgltf_result_success;
}

void release_game_file(const cgltf_memory_options* /*memory*/, const cgltf_file_options* /*file*/, void* data) {
  std::free(data);
}

cgltf_options game_file_options() {
  cgltf_options options{};
  options.file.read = read_game_file;
  options.file.release = release_game_file;
  return options;
}

// The file parsed; null (and why, in `error`) when it cannot be, or is too big for memory alone.
CgltfPtr parsed_gltf(const std::string& path, std::string& error, std::uint64_t max_bytes) {
  if (!sound(gltf_budget::file_problem(path), path, error)) return nullptr;
  if (!sound(file_exceeds_budget(path, max_bytes) ? "scene exceeds its loading byte budget" : "", path, error))
    return nullptr;
  cgltf_options options = game_file_options();
  cgltf_data* raw = nullptr;
  const cgltf_result parsed = cgltf_parse_file(&options, path.c_str(), &raw);
  CgltfPtr data(raw);
  if (!succeeded(parsed, "cannot parse", path, error)) return nullptr;
  return data;
}

// The parsed file's buffers loaded, and the whole checked; false (and why, in `error`) when any of
// that fails (a scene too big for memory is refused before its data files are read). The loader's
// own checks come before the library's: those do sums that a file can make wrap around, and then
// read where the wrapped sum points.
bool loaded_and_checked(cgltf_data& data, const gltf_checks::NamedFiles& files, const std::string& path,
                        std::string& error, std::uint64_t max_bytes) {
  cgltf_options options = game_file_options();
  return sound(gltf_checks::buffer_file_problem(data, files), path, error) &&
         sound(gltf_budget::scene_problem(data, path, files), path, error) &&
         sound(gltf_budget::scene_bytes(data, path, files) > max_bytes ? "scene exceeds its loading byte budget" : "",
               path, error) &&
         succeeded(cgltf_load_buffers(&options, &data, path.c_str()), "cannot load buffers of", path, error) &&
         sound(gltf_checks::data_problem(data), path, error) &&
         succeeded(cgltf_validate(&data), "invalid glTF", path, error);
}

bool pictures_checked(const cgltf_data& data, const gltf_checks::NamedFiles& files, const std::string& path,
                      LoadResult& result, std::uint64_t max_bytes) {
  if (!sound(gltf_budget::pictures_problem(data, path, files, result.scene.images), path, result.error)) return false;
  const auto bytes = gltf_budget::scene_bytes(data, path, files) + gltf_budget::decoded_bytes(result.scene.images);
  return sound(bytes > max_bytes ? "scene exceeds its loading byte budget" : "", path, result.error);
}

}  // namespace

namespace {
LoadResult read_scene(const CgltfPtr& data, const std::string& path, LoadResult result, std::uint64_t max_bytes) {
  if (data == nullptr) return result;
  // (Each file the scene names is found once, for every step below.)
  const gltf_checks::NamedFiles files = gltf_checks::named_files(*data, folder_of(path));
  if (!loaded_and_checked(*data, files, path, result.error, max_bytes)) return result;
  GltfReader reader(*data, files.images, result);
  // (Its pictures are decoded later, once shown: what that will take is known from their headers.)
  result.ok = sound(reader.run(), path, result.error) && pictures_checked(*data, files, path, result, max_bytes);
  if (!result.ok) result.scene = SceneDesc{};
  return result;
}
}  // namespace
LoadResult load_gltf_scene(const std::string& path, std::uint64_t max_bytes) {
  LoadResult result;
  const auto data = parsed_gltf(path, result.error, max_bytes);
  return read_scene(data, path, std::move(result), max_bytes);
}
LoadResult load_gltf_snapshot(const std::string& path, const std::vector<std::uint8_t>& bytes,
                              std::uint64_t max_bytes) {
  LoadResult result;
  if (!sound(bytes.size() > max_bytes ? "scene exceeds its loading byte budget" : "", path, result.error))
    return result;
  cgltf_options options = game_file_options();
  cgltf_data* raw = nullptr;
  const auto parsed = cgltf_parse(&options, bytes.data(), bytes.size(), &raw);
  const CgltfPtr data(raw);
  if (!succeeded(parsed, "cannot parse", path, result.error)) return result;
  return read_scene(data, path, std::move(result), max_bytes);
}

LoadResult load_gltf_scene(const std::string& path) { return load_gltf_scene(path, gltf_budget::kMaxSceneBytes); }

}  // namespace redgem
