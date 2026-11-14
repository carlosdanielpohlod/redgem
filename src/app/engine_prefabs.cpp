#include "app/engine.hpp"

#include <stdexcept>

#include "core/safe_path.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

constexpr std::size_t kMaxRuntimePrefabFiles = 32;
constexpr std::size_t kMaxRuntimePrefabNodes = 1024;
constexpr std::uint64_t kMaxRuntimePrefabBytes = 64 * prefabs::Limits::kMebibyte;

std::string prefab_path(const std::string& name) {
  if (!safe_asset_name(name)) throw std::invalid_argument("prefab path must stay inside prefabs/");
  const auto path = std::filesystem::u8path(name).lexically_normal();
  if (path.extension() != ".glb") throw std::invalid_argument("prefab file must be a .glb");
  return (std::filesystem::path(config::kPrefabsDirName) / path).generic_u8string();
}

SceneDesc prefab_reference(const std::string& file) {
  NodeDesc root;
  root.props[prefabs::kFileProperty] = file;
  SceneDesc reference;
  reference.nodes.push_back(std::move(root));
  return reference;
}

void require_rigid_prefab(const SceneDesc& scene) {
  const bool rigid = scene.skins.empty() && scene.animations.empty();
  if (!rigid) {
    throw std::invalid_argument("Scene.spawn_prefab requires a rigid prefab without skins or animation");
  }
}

prefabs::Source read_prefab(const std::string& file, const std::filesystem::path& game_dir, std::uint64_t available) {
  prefabs::Limits limits;
  limits.nodes = kMaxRuntimePrefabNodes;
  limits.expanded_bytes = available;
  limits.input_bytes = available;
  prefabs::Source prefab;
  prefab.scene = prefabs::resolve(prefab_reference(file), game_dir, limits);
  require_rigid_prefab(prefab.scene);
  for (auto& node : prefab.scene.nodes) node.props.erase(props::kPrefab);
  return prefab;
}

}  // namespace

const prefabs::Source& Engine::load_runtime_prefab(const std::string& file) {
  const auto found = runtime_prefabs_.find(file);
  if (found != runtime_prefabs_.end()) return found->second;
  if (runtime_prefabs_.size() >= kMaxRuntimePrefabFiles) throw std::runtime_error("too many runtime prefab files");
  const auto available = kMaxRuntimePrefabBytes - runtime_prefab_bytes_;
  auto prefab = read_prefab(file, options_.game_dir, available);
  const auto bytes = prefabs::asset_bytes(prefab.scene) + prefabs::instance_bytes(prefab.scene);
  if (bytes > available) throw std::runtime_error("runtime prefab asset budget exceeded");
  prefabs::transfer_assets(scene_desc_, prefab);
  runtime_prefab_bytes_ += bytes;
  ++scene_generation_;
  return runtime_prefabs_.emplace(file, std::move(prefab)).first->second;
}

ObjectId Engine::spawn_prefab(const std::string& name, const SpawnRequest& request) {
  const auto& prefab = load_runtime_prefab(prefab_path(name));
  return finish_spawn(scene_.instantiate_rigid(prefab.scene.nodes, prefab.assets.meshes, scene_desc_), request);
}

}  // namespace redgem
