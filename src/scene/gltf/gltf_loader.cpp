#include "scene/gltf/gltf_loader.hpp"

#include <cgltf.h>

#include <algorithm>
#include <filesystem>
#include <memory>

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

// The file parsed; null (and why, in `error`) when it cannot be, or is too big for memory alone.
CgltfPtr parsed_gltf(const std::string& path, std::string& error) {
  if (!sound(gltf_budget::file_problem(path), path, error)) return nullptr;
  cgltf_options options{};
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
                        std::string& error) {
  cgltf_options options{};
  return sound(gltf_checks::buffer_file_problem(data, files), path, error) &&
         sound(gltf_budget::scene_problem(data, path, files), path, error) &&
         succeeded(cgltf_load_buffers(&options, &data, path.c_str()), "cannot load buffers of", path, error) &&
         sound(gltf_checks::data_problem(data), path, error) &&
         succeeded(cgltf_validate(&data), "invalid glTF", path, error);
}

}  // namespace

LoadResult load_gltf_scene(const std::string& path) {
  LoadResult result;
  const CgltfPtr data = parsed_gltf(path, result.error);
  if (data == nullptr) return result;
  // (Each file the scene names is found once, for every step below.)
  const gltf_checks::NamedFiles files = gltf_checks::named_files(*data, folder_of(path));
  if (!loaded_and_checked(*data, files, path, result.error)) return result;
  GltfReader reader(*data, files.images, result);
  // (Its pictures are decoded later, once shown: what that will take is known from their headers.)
  result.ok = sound(reader.run(), path, result.error) &&
              sound(gltf_budget::pictures_problem(*data, path, files, result.scene.images), path, result.error);
  if (!result.ok) result.scene = SceneDesc{};
  return result;
}

}  // namespace redgem
