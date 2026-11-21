// Part of EditorSession: models imported into the scene with their import settings, and imported
// again when those settings change.

#include "core/config.hpp"
#include "core/import_settings.hpp"
#include "core/log.hpp"
#include "editor/editor_session.hpp"
#include "editor/model_import_settings.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

namespace fs = std::filesystem;

constexpr const char* kStopToEdit = "stop the game to edit the scene";

int index_with_id(const SceneDocument& document, const std::string& id) {
  for (int index = 0; index < document.node_count(); ++index) {
    if (document.node(index).stable_id == id) return index;
  }
  return kNoIndex;
}

std::vector<std::string> child_ids(const SceneDocument& document, const std::string& id) {
  std::vector<std::string> ids;
  for (int child : document.node(index_with_id(document, id)).children) ids.push_back(document.node(child).stable_id);
  return ids;
}

// A path inside `folder`, '/' between its folders; empty for one outside it.
std::string inside(const fs::path& folder, const fs::path& path) {
  const fs::path relative = path.lexically_normal().lexically_relative(folder.lexically_normal());
  const bool outside = relative.empty() || *relative.begin() == "..";
  return outside ? std::string() : relative.generic_string();
}

void extract_pictures(const fs::path& path, const fs::path& images_dir) {
  std::string error;
  const int written = model_settings::extract_pictures(path, images_dir, error);
  if (written < 0) return log::error("cannot extract the pictures of " + path.filename().string() + ": " + error);
  const fs::path folder = model_settings::pictures_dir(images_dir, path);
  log::info("extracted " + std::to_string(written) + " pictures into " + folder.string());
}

}  // namespace

ObjectId EditorSession::import_model(const fs::path& path, Vec3 position, std::string& error, ObjectId parent) {
  if (!editing()) {
    error = kStopToEdit;
    return kNoObject;
  }
  const int node = add_model(path, error, parent);
  if (node == kNoIndex) return kNoObject;
  log::info("imported " + path.filename().string());
  if (parent == kNoObject) return place_new(node, position);
  refresh_preview();  // the parent's world placement is needed below
  Transform world = preview_.world_transform(node);
  world.position = position;
  return place_new(node, relative_to(preview_.world_transform(parent), world).position);
}

int EditorSession::add_model(const fs::path& path, std::string& error, int parent) {
  const int node = document_.import_model(path, error, parent);
  if (node == kNoIndex) return kNoIndex;
  const import_settings::ModelSettings settings = import_settings::model_settings(path);
  model_settings::apply(document_, node, settings);
  tag_model(node, path);
  if (settings.materials == import_settings::Materials::Extract)
    extract_pictures(path, game_dir_ / config::kImagesDirName);
  return node;
}

void EditorSession::tag_model(int node, const fs::path& path) {
  const std::string model_file = inside(game_dir_ / config::kModelsDirName, path);
  if (!model_file.empty()) document_.set_prop(node, props::kModelFile, model_file, false);
}

int EditorSession::reimport_model(const std::string& model_file, std::string& error) {
  if (!editing()) {
    error = kStopToEdit;
    return 0;
  }
  const fs::path path = game_dir_ / config::kModelsDirName / model_file;
  int done = 0;
  for (const std::string& id : model_copies(model_file)) {
    if (!reimport_copy(id, path, error)) break;
    ++done;
  }
  selection_.clear();  // (the objects it held are gone)
  refresh_preview();
  return done;
}

std::vector<std::string> EditorSession::model_copies(const std::string& model_file) const {
  std::vector<std::string> ids;
  for (int index = 0; index < document_.node_count(); ++index) {
    const NodeDesc& node = document_.node(index);
    if (get_string(node.props, props::kModelFile) == model_file) ids.push_back(node.stable_id);
  }
  return ids;
}

bool EditorSession::reimport_copy(const std::string& root_id, const fs::path& path, std::string& error) {
  const int root = index_with_id(document_, root_id);
  const int fresh = add_model(path, error, document_.node(root).parent);
  if (fresh == kNoIndex) return false;
  const std::string fresh_id = document_.node(fresh).stable_id;
  // (Nothing to make: the import above is the step, and a failed swap takes it back.)
  return document_.make_then_edit([] {}, [&] { return swap_model_nodes(root_id, fresh_id); }, error);
}

std::string EditorSession::swap_model_nodes(const std::string& root_id, const std::string& fresh_id) {
  const std::vector<std::string> held = child_ids(document_, root_id);
  for (const std::string& child : child_ids(document_, fresh_id)) {
    const int index = index_with_id(document_, child);
    const Transform local = document_.node(index).local;
    document_.reparent(index, index_with_id(document_, root_id), true, &local);
  }
  std::vector<int> gone{index_with_id(document_, fresh_id)};
  for (const std::string& id : held) gone.push_back(index_with_id(document_, id));
  document_.remove(gone);
  return {};
}

}  // namespace redgem
