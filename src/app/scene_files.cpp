#include "app/scene_files.hpp"

#include <utility>

#include "core/config.hpp"
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "scene/gltf/gltf_loader.hpp"

namespace redgem {
namespace {

constexpr const char* kLinkedScene = "the scene is a symbolic link: built games do not carry links";

// The scene of a file, with the loader's warnings logged; the error when it cannot be read.
std::variant<SceneDesc, std::string> read_scene_file(const std::filesystem::path& file) {
  LoadResult loaded = load_gltf_scene(file.string());
  if (!loaded.ok) return loaded.error;
  for (const std::string& warning : loaded.warnings) log::warning(warning);
  return std::move(loaded.scene);
}

// Where the scene a script calls `name` is in a game's folder.
std::filesystem::path scene_in_game(const std::string& name) {
  if (name == config::kMainSceneName) return config::kSceneFileName;
  return std::filesystem::path(config::kScenesDirName) / (name + config::kSceneExtension);
}

}  // namespace

bool is_game_folder(const std::filesystem::path& folder) {
  std::error_code fs_error;
  return std::filesystem::exists(folder / config::kSceneFileName, fs_error);
}

std::variant<SceneDesc, std::string> read_game_scene(const std::filesystem::path& game_dir) {
  if (through_link(game_dir, config::kSceneFileName)) return std::string(kLinkedScene);
  return read_scene_file(game_dir / config::kSceneFileName);
}

std::filesystem::path scene_file(const std::filesystem::path& game_dir, const std::string& name) {
  return game_dir / scene_in_game(name);
}

bool safe_scene_name(const std::string& name) {
  if (safe_asset_name(name)) return true;
  warn_of_unsafe_asset_name(name);
  return false;
}

std::optional<SceneDesc> read_scene(const std::filesystem::path& game_dir, const std::string& name) {
  if (!safe_scene_name(name)) return std::nullopt;
  if (through_link(game_dir, scene_in_game(name))) return std::nullopt;
  auto scene = read_scene_file(scene_file(game_dir, name));
  if (auto* loaded = std::get_if<SceneDesc>(&scene)) return std::move(*loaded);
  log::error("cannot load scene '" + name + "': " + std::get<std::string>(scene));
  return std::nullopt;
}

}  // namespace redgem
