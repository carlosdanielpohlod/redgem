#include "editor/model_import_settings.hpp"

#include <system_error>
#include <vector>

#include "core/files.hpp"
#include "core/math.hpp"
#include "editor/document/scene_document.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/props.hpp"

namespace redgem::model_settings {
namespace {

namespace fs = std::filesystem;
using import_settings::Colliders;
using import_settings::ModelSettings;
using import_settings::UpAxis;

constexpr float kQuarterTurn = kPi / 2.0f;
constexpr Vec3 kTiltAxis{1.0f, 0.0f, 0.0f};
constexpr const char* kNumberMark = "_";
constexpr int kNotExtracted = -1;

// What turns a model made with Z up so that its up is Y (+Z goes to +Y).
Quat upright(UpAxis axis) {
  return axis == UpAxis::Z ? quat_from_axis_angle(kTiltAxis, -kQuarterTurn) : Quat::identity();
}

const char* collider_word(Colliders colliders) {
  return colliders == Colliders::Mesh ? props::kColliderMesh : props::kColliderBox;
}

// `root` and every node under it, parents first.
std::vector<int> subtree_of(const SceneDocument& document, int root) {
  std::vector<int> nodes{root};
  for (std::size_t next = 0; next < nodes.size(); ++next) {
    for (int child : document.node(nodes[next]).children) nodes.push_back(child);
  }
  return nodes;
}

void give_colliders(SceneDocument& document, int root, Colliders colliders) {
  if (colliders == Colliders::None) return;
  for (int node : subtree_of(document, root)) {
    if (document.node(node).mesh != kNoIndex) {
      document.set_prop(node, props::kCollider, std::string(collider_word(colliders)), false);
    }
  }
}

// Writes one picture unless its file is already there; whether it was written (false with
// `error` set when it could not be).
bool write_picture(const fs::path& file, const ImageData& image, std::string& error) {
  std::error_code fs_error;
  const bool nothing_to_write = image.encoded.empty() || fs::exists(file, fs_error);
  if (nothing_to_write) return false;
  fs::create_directories(file.parent_path(), fs_error);
  return write_bytes_file(file, image.encoded, error);
}

// The file of the model's picture `number`: <model>_<number>.<kind>.
fs::path picture_file(const fs::path& folder, const fs::path& model, std::size_t number, const ImageData& image) {
  return folder / (model.stem().string() + kNumberMark + std::to_string(number) + image.file_extension);
}

int write_pictures(const std::vector<ImageData>& images, const fs::path& model, const fs::path& folder,
                   std::string& error) {
  int written = 0;
  for (std::size_t number = 0; number < images.size(); ++number) {
    if (write_picture(picture_file(folder, model, number, images[number]), images[number], error)) ++written;
    if (!error.empty()) return kNotExtracted;
  }
  return written;
}

}  // namespace

Transform settled(const Transform& local, const ModelSettings& settings) {
  const Quat turn = upright(settings.up_axis);
  Transform placed = local;
  placed.position = rotate(turn, local.position * settings.scale);
  placed.rotation = normalize(turn * local.rotation);
  placed.scale = local.scale * settings.scale;
  return placed;
}

void apply(SceneDocument& document, int root, const ModelSettings& settings) {
  for (int top : document.node(root).children) {
    document.set_local(top, settled(document.node(top).local, settings), false);
  }
  give_colliders(document, root, settings.colliders);
}

fs::path pictures_dir(const fs::path& images_dir, const fs::path& model) { return images_dir / model.stem(); }

int extract_pictures(const fs::path& model, const fs::path& images_dir, std::string& error) {
  const LoadResult loaded = load_gltf_scene(model.string());
  if (!loaded.ok) {
    error = loaded.error;
    return kNotExtracted;
  }
  return write_pictures(loaded.scene.images, model, pictures_dir(images_dir, model), error);
}

}  // namespace redgem::model_settings
