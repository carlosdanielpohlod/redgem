#include "scene/prefab_import.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/scene_ids.hpp"
#include "scene/gltf/gltf_checks.hpp"

namespace redgem::prefabs {
namespace {
namespace fs = std::filesystem;
constexpr const char* kExtension = ".glb";
void require(bool valid, const char* reason) {
  if (!valid) throw std::runtime_error(reason);
}
void reserve_bytes(std::uint64_t& used, std::uint64_t amount, std::uint64_t limit) {
  require(used <= limit && amount <= limit - used, "prefab expansion exceeds its memory budget");
  used += amount;
}
bool has_file(const NodeDesc& node) { return node.props.count(kFileProperty) != 0; }
std::string reference(const NodeDesc& node) {
  const auto value = get_string(node.props, kFileProperty);
  if (!value || value->empty()) throw std::runtime_error("prefab_file must be a nonempty game-relative GLB path");
  require(safe_asset_name(*value), "unsafe prefab file path");
  return *value;
}
void require_root(const SceneDesc& scene) {
  require(!scene.nodes.empty(), "prefab file is empty");
  require(scene.nodes.front().parent == kNoIndex, "prefab file has no root");
  for (std::size_t i = 1; i < scene.nodes.size(); ++i)
    require(scene.nodes[i].parent != kNoIndex, "prefab files must have exactly one root");
  for (const auto& animation : scene.animations)
    for (const auto& channel : animation.channels)
      require(channel.node != 0, "animate prefab descendants instead of the prefab root");
}
bool empty_reference(const NodeDesc& node) {
  const bool no_geometry = node.mesh == kNoIndex && node.skin == kNoIndex;
  return no_geometry && !node.camera && !node.light;
}
bool identity_transform(const Transform& value) {
  return length(value.position) == 0 && length(value.scale - Vec3::one()) == 0 && value.rotation.x == 0 &&
         value.rotation.y == 0 && value.rotation.z == 0 && std::fabs(value.rotation.w) == 1;
}
void check_hierarchy(const SceneDesc& scene) {
  std::vector<int> depths(scene.nodes.size(), 1);
  for (std::size_t i = 0; i < scene.nodes.size(); ++i) {
    const int parent = scene.nodes[i].parent;
    require(parent == kNoIndex || (parent >= 0 && static_cast<std::size_t>(parent) < i), "invalid prefab hierarchy");
    if (parent != kNoIndex) depths[i] += depths[parent];
    require(depths[i] <= gltf_checks::kMaxNodeDepth, "expanded prefab hierarchy is too deep");
  }
}
class Resolver {
 public:
  Resolver(SceneDesc source, fs::path folder, const Limits& limits)
      : scene_(std::move(source)), folder_(std::move(folder)), limits_(limits) {}
  SceneDesc run() {
    const auto authored = scene_.nodes.size();
    require(authored <= limits_.nodes, "prefab scene has too many nodes");
    for (const auto& warning : assign_scene_ids(scene_)) log::warning(warning);
    for (const auto& node : scene_.nodes) identities_.insert(node.stable_id);
    remember_animation_targets(0);
    check_hierarchy(scene_);
    for (std::size_t id = 0; id < authored; ++id) expand(static_cast<int>(id));
    check_hierarchy(scene_);
    return std::move(scene_);
  }

 private:
  void remember_animation_targets(std::size_t first) {
    for (std::size_t i = first; i < scene_.animations.size(); ++i)
      for (const auto& channel : scene_.animations[i].channels) animated_.insert(channel.node);
  }
  fs::path file(const std::string& uri) const {
    require(safe_asset_name(uri), "unsafe prefab file path");
    const auto found = file_inside(folder_, fs::u8path(uri));
    require(!through_link(folder_, fs::u8path(uri)), "prefab file uses a symbolic link");
    if (!found) throw std::runtime_error("prefab file is missing, linked or outside the game");
    require(found->extension() == kExtension, "prefab file must be a GLB");
    return fs::canonical(*found);
  }
  std::string key(const fs::path& path) const {
    return path.lexically_relative(fs::canonical(folder_)).generic_u8string();
  }
  Source read_source(const fs::path& path) {
    auto loaded = load_gltf_scene(path.u8string(), limits_.expanded_bytes - expanded_bytes_);
    if (!loaded.ok) throw std::runtime_error(loaded.error);
    require_root(loaded.scene);
    for (const auto& warning : loaded.warnings) log::warning(warning);
    Source entry;
    entry.instance_bytes = instance_bytes(loaded.scene);
    entry.asset_bytes = asset_bytes(loaded.scene);
    entry.scene = std::move(loaded.scene);
    return entry;
  }
  Source& source(const fs::path& path) {
    const auto name = key(path);
    const auto found = sources_.find(name);
    if (found != sources_.end()) return found->second;
    require(sources_.size() < limits_.files, "too many prefab files");
    reserve_bytes(input_bytes_, fs::file_size(path), limits_.input_bytes);
    return sources_.emplace(name, read_source(path)).first->second;
  }
  void enter(const fs::path& path) {
    require(stack_.size() < limits_.depth, "prefab nesting is too deep");
    for (const auto& active : stack_) require(!fs::equivalent(active, path), "prefab reference cycle");
    require(instances_ < limits_.instances, "too many prefab instances");
    ++instances_;
    stack_.push_back(path);
  }
  void reserve(const Source& source) {
    const auto added = source.scene.nodes.size() - 1;
    require(scene_.nodes.size() <= limits_.nodes && added <= limits_.nodes - scene_.nodes.size(),
            "too many prefab nodes");
    reserve_bytes(expanded_bytes_, source.instance_bytes, limits_.expanded_bytes);
    if (!source.transferred) reserve_bytes(expanded_bytes_, source.asset_bytes, limits_.expanded_bytes);
  }
  void keep_identities(const Instance& instance, std::size_t count) {
    for (std::size_t i = 1; i < count; ++i) {
      const auto& identity = scene_.nodes[instance.nodes + i - 1].stable_id;
      require(identities_.insert(identity).second, "prefab identity collision");
    }
  }
  void remember_imported_animation_targets(const Source& source, const Instance& instance) {
    for (const auto& animation : source.scene.animations)
      for (const auto& channel : animation.channels) animated_.insert(instance_node(channel.node, instance));
  }
  Instance import(Source& prefab, int id, const std::string& uri) {
    require(animated_.count(id) == 0 || identity_transform(prefab.scene.nodes.front().local),
            "animated prefab placements require an identity source root transform");
    reserve(prefab);
    transfer_assets(scene_, prefab);
    const auto instance = append_instance(scene_, prefab, id, uri);
    remember_imported_animation_targets(prefab, instance);
    keep_identities(instance, prefab.scene.nodes.size());
    return instance;
  }
  void expand(int id) {
    if (!has_file(scene_.nodes[id])) return;
    require(empty_reference(scene_.nodes[id]), "a prefab reference must be an empty node");
    const auto uri = fs::u8path(reference(scene_.nodes[id])).lexically_normal().generic_u8string();
    const auto path = file(uri);
    enter(path);
    auto& prefab = source(path);
    const auto instance = import(prefab, id, uri);
    expand(id);  // a prefab's own root may reference another file
    for (std::size_t i = 1; i < prefab.scene.nodes.size(); ++i) expand(instance.nodes + static_cast<int>(i) - 1);
    stack_.pop_back();
  }
  SceneDesc scene_;
  fs::path folder_;
  Limits limits_;
  std::map<std::string, Source> sources_;
  std::vector<fs::path> stack_;
  std::set<std::string> identities_;
  std::set<int> animated_;
  std::uint64_t input_bytes_ = 0;
  std::uint64_t expanded_bytes_ = 0;
  std::size_t instances_ = 0;
};
}  // namespace
bool has_references(const SceneDesc& scene) { return std::any_of(scene.nodes.begin(), scene.nodes.end(), has_file); }
SceneDesc resolve(SceneDesc scene, const std::filesystem::path& game_dir, const Limits& limits) {
  if (!has_references(scene)) return scene;
  return Resolver(std::move(scene), fs::canonical(game_dir), limits).run();
}
}  // namespace redgem::prefabs
