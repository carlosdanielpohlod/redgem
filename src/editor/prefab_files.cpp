#include "editor/prefab_files.hpp"

#include <algorithm>
#include <exception>
#include <set>
#include <stdexcept>
#include <utility>
#include "core/config.hpp"
#include "core/safe_path.hpp"
#include "editor/node_tree.hpp"
#include "scene/prefab_import.hpp"
#include "scene/scene_ids.hpp"
#include "storage/durable_file.hpp"

namespace redgem::prefab_files {
namespace {
namespace fs = std::filesystem;
constexpr std::size_t kNameBytes = 64;                   // short, portable file stems
constexpr const char* kStagePrefix = ".redgem-prefab-";  // hidden folders never ship with a game
constexpr const char* kStageFile = "source.glb";

void require(bool condition, const char* reason) {
  if (!condition) throw std::runtime_error(reason);
}
bool name_character(char letter) {
  const bool lowercase = letter >= 'a' && letter <= 'z';
  const bool digit = letter >= '0' && letter <= '9';
  return lowercase || digit || letter == '_' || letter == '-';
}
fs::path destination(const fs::path& game, const std::string& name) {
  require(valid_name(name), "use a lowercase name starting with a letter (letters, digits, _ and -; at most 64 bytes)");
  const fs::path relative = fs::path(config::kPrefabsDirName) / (name + config::kSceneExtension);
  require(!through_link(game, relative), "prefab destination uses a symbolic link");
  const fs::path file = game / relative;
  require(!fs::exists(fs::symlink_status(file)), "a file with this prefab name already exists");
  fs::create_directories(file.parent_path());
  return file;
}
void require_contained_skin(const SceneDesc& scene, int skin, const std::set<int>& kept) {
  if (skin == kNoIndex) return;
  for (const int joint : scene.skins.at(static_cast<std::size_t>(skin)).joints)
    require(kept.count(joint) != 0, "select a hierarchy containing every joint of its skeleton");
}
void require_exportable(const SceneDesc& scene, int root) {
  const auto copied = prefabs::instance_bytes(scene) + prefabs::asset_bytes(scene);
  require(copied <= prefabs::Limits::kDefaultExpandedBytes, "scene exceeds the prefab export memory budget");
  const auto nodes = node_tree::subtree(scene.nodes, root);
  const std::set<int> kept(nodes.begin(), nodes.end());
  for (const int node : nodes) require_contained_skin(scene, scene.nodes.at(node).skin, kept);
  for (const auto& clip : scene.animations)
    for (const auto& channel : clip.channels)
      require(channel.node != root, "animate descendants instead of the prefab root before exporting");
}
struct Stage {
  fs::path dir;
  explicit Stage(const fs::path& game) : dir(game / (std::string(kStagePrefix) + make_scene_id())) {
    require(fs::create_directory(dir), "cannot create prefab staging folder");
  }
  ~Stage() {
    std::error_code ignored;
    fs::remove_all(dir, ignored);
  }
  Stage(const Stage&) = delete;
  Stage& operator=(const Stage&) = delete;
};
void validate_source(const fs::path& game, const fs::path& staged) {
  SceneDesc probe;
  NodeDesc node;
  node.props[props::kPrefabFile] = staged.lexically_relative(game).generic_u8string();
  probe.nodes.push_back(std::move(node));
  // The runtime checks the single root, nested dependencies, animations and memory bounds.
  prefabs::resolve(std::move(probe), game);
}
void publish(const fs::path& staged, const fs::path& target) {
  // Both paths are on the game's filesystem. Linking publishes the complete file atomically
  // and refuses an existing destination, including one created since the name was checked.
  fs::create_hard_link(staged, target);
  if (!durable::sync_folder(target.parent_path()))
    throw std::runtime_error("prefab was written, but its folder could not be flushed");
}
bool save_source(SceneDocument& document, int root, Transform placement, const fs::path& game, const fs::path& target,
                 std::string& error) {
  require_exportable(document.desc(), root);
  placement.position = Vec3::zero();
  const auto bytes = document.copy_nodes({root}, error, {prefabs::checked_placement(placement)});
  if (!bytes) return false;
  require(bytes->size() <= prefabs::Limits::kDefaultInputBytes, "prefab file exceeds its input byte budget");
  const Stage stage(game);
  const auto staged = stage.dir / kStageFile;
  const std::string contents(bytes->begin(), bytes->end());
  require(durable::write_flushed(staged, contents), "cannot write and flush the prefab source");
  validate_source(game, staged);
  publish(staged, target);
  return true;
}
}  // namespace
bool valid_name(const std::string& name) {
  if (name.empty() || name.size() > kNameBytes) return false;
  if (name.front() < 'a' || name.front() > 'z') return false;
  return std::all_of(name.begin(), name.end(), name_character);
}
bool save(SceneDocument& document, int root, const Transform& placement, const fs::path& game, const std::string& name,
          std::string& error) {
  error.clear();
  try {
    require(document.has_node(root), "select one hierarchy to export as a prefab");
    const auto folder = fs::canonical(game);
    const auto target = destination(folder, name);
    return save_source(document, root, placement, folder, target, error);
  } catch (const std::exception& problem) {
    error = problem.what();
    return false;
  }
}
}  // namespace redgem::prefab_files
