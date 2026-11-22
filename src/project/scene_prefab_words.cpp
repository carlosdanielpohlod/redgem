#include "project/scene_prefab_words.hpp"

#include <exception>

#include "app/game_settings.hpp"
#include "core/config.hpp"
#include "core/translations.hpp"
#include "project/scene_assignments.hpp"
#include "scene/prefabs.hpp"

namespace redgem::tools {
namespace {
constexpr const char* kRemovedWord = "removed=true";
// What `scene set` changes and a part cannot override: what it is called and where it hangs.
constexpr const char* kRefusedPrefixes[] = {"name=", "parent="};
constexpr const char* kScratchName = "part";

bool same(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool same(Quat a, Quat b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
bool same_light(const LightDesc& a, const LightDesc& b) {
  const bool same_color = a.color.r == b.color.r && a.color.g == b.color.g && a.color.b == b.color.b;
  const bool same_cones = a.inner_cone == b.inner_cone && a.outer_cone == b.outer_cone;
  return a.type == b.type && same_color && a.intensity == b.intensity && a.range == b.range && same_cones;
}
bool same_camera(const CameraDesc& a, const CameraDesc& b) {
  return a.fov_y_radians == b.fov_y_radians && a.near_plane == b.near_plane && a.far_plane == b.far_plane;
}

// A node like the part, in a document of its own, for `scene set`'s words to change.
int scratch_kind(SceneDocument& scratch, const NodeDesc& part) {
  if (part.light) {
    const int node = scratch.add_light(part.light->type, kScratchName, part.light->color, part.light->intensity);
    scratch.set_light(node, *part.light, false);
    return node;
  }
  if (!part.camera) return scratch.add_empty(kScratchName);
  const int node = scratch.add_camera(kScratchName, part.camera->fov_y_radians);
  scratch.set_camera(node, *part.camera, false);
  return node;
}
int scratch_node(SceneDocument& scratch, const NodeDesc& part) {
  const int node = scratch_kind(scratch, part);
  scratch.set_local(node, part.local, false);
  for (const auto& [key, value] : part.props) scratch.set_prop(node, key, value, false);
  return node;
}

void diff_transform(const Transform& before, const Transform& after, prefabs::PartOverride& change) {
  if (!same(before.position, after.position)) change.position = after.position;
  if (!same(before.rotation, after.rotation)) change.rotation = after.rotation;
  if (!same(before.scale, after.scale)) change.scale = after.scale;
}
void diff_props(const PropertyMap& before, const PropertyMap& after, prefabs::PartOverride& change) {
  for (const auto& [key, value] : after) {
    const auto old = before.find(key);
    if (old == before.end() || !(old->second == value)) change.props.emplace(key, value);
  }
  for (const auto& entry : before) {
    if (after.count(entry.first) == 0) change.unset.insert(entry.first);
  }
}
void diff_components(const NodeDesc& before, const NodeDesc& after, prefabs::PartOverride& change) {
  if (after.light && before.light && !same_light(*before.light, *after.light)) change.light = after.light;
  if (after.camera && before.camera && !same_camera(*before.camera, *after.camera)) change.camera = after.camera;
}
bool refused_word(const std::string& word) {
  for (const char* prefix : kRefusedPrefixes) {
    if (word.rfind(prefix, 0) == 0) return true;
  }
  return false;
}
// Applies the words to the scratch node; what is wrong with the first that is.
std::string apply_words(SceneEdit& edit, int& node, const std::vector<std::string>& words, bool& removed) {
  for (const auto& word : words) {
    if (word == kRemovedWord) {
      removed = true;
      continue;
    }
    if (refused_word(word)) return tr("project.scene.override_refused", {{"word", word}});
    const auto problem = apply_assignment(edit, node, word);
    if (!problem.empty()) return problem;
  }
  return {};
}
std::string path_text(const std::string& path) { return path.empty() ? kPlacementRootWord : path; }

// The scene with the placement's override of `path` taken out.
SceneDesc without_override(const SceneDocument& document, int placement, const std::string& path) {
  SceneDesc scene = document.desc();
  auto overrides = prefabs::read_overrides(scene.nodes.at(static_cast<std::size_t>(placement)).props);
  overrides.erase(path);
  prefabs::write_overrides(scene.nodes.at(static_cast<std::size_t>(placement)).props, overrides);
  return scene;
}
}  // namespace

std::optional<NodeDesc> inherited_part(const SceneDocument& document, int placement,
                                       const std::filesystem::path& game_dir, const std::string& path, bool own,
                                       std::string& error) {
  try {
    prefabs::Origins origins;
    const auto scene = own ? document.desc() : without_override(document, placement, path);
    const auto resolved = prefabs::resolve(scene, game_dir, read_prefab_limits(game_dir), &origins);
    const auto origin = origins.find(placement);
    const auto part = origin == origins.end() ? nullptr : &origin->second.parts;
    if (part && part->count(path) != 0) return resolved.nodes.at(static_cast<std::size_t>(part->at(path)));
    error = tr("project.scene.no_part", {{"part", path}});
  } catch (const std::exception& problem) {
    error = problem.what();
  }
  return std::nullopt;
}

std::optional<prefabs::PartOverride> part_change(const NodeDesc& current, const NodeDesc& base,
                                                 const std::vector<std::string>& words,
                                                 const std::filesystem::path& scripts_dir, std::string& error,
                                                 std::vector<std::string>& notes) {
  auto scratch = SceneDocument::create_empty(scripts_dir / kScratchName);
  const NodeDesc before = scratch.node(scratch_node(scratch, base));
  int node = scratch_node(scratch, current);
  SceneEdit edit{scratch, scripts_dir, {}};
  prefabs::PartOverride change;
  error = apply_words(edit, node, words, change.removed);
  notes = std::move(edit.notes);
  if (!error.empty()) return std::nullopt;
  const NodeDesc& after = scratch.node(node);
  diff_transform(before.local, after.local, change);
  diff_props(before.props, after.props, change);
  diff_components(before, after, change);
  return change;
}

std::optional<prefabs::PartOverride> placement_part_change(const PlacedPart& part,
                                                           const std::vector<std::string>& words, std::string& error,
                                                           std::vector<std::string>& notes) {
  const auto current = inherited_part(part.document, part.placement, part.game_dir, part.path, true, error);
  const auto base =
      current ? inherited_part(part.document, part.placement, part.game_dir, part.path, false, error) : current;
  if (!base) return std::nullopt;
  auto change = part_change(*current, *base, words, part.game_dir / config::kScriptsDirName, error, notes);
  const auto overrides = prefab_instances::overrides_of(part.document, part.placement);
  const auto old = overrides.find(part.path);
  if (change && old != overrides.end()) change->removed = change->removed || old->second.removed;
  return change;
}

std::string overrides_listing(const std::vector<prefab_instances::Field>& fields) {
  std::string text;
  for (const auto& field : fields) text += path_text(field.path) + " " + field.name + "=" + field.value + "\n";
  return text;
}

}  // namespace redgem::tools
