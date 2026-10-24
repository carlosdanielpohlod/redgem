#include "editor/game_scenes.hpp"

#include <algorithm>
#include <cctype>

#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "editor/new_objects.hpp"

namespace redgem {
namespace {

// What a new scene starts with besides what the GameObject menu makes (see editor/new_objects.hpp).
constexpr const char* kGroundName = "Ground";
constexpr Vec3 kGroundScale{2.0f, 1.0f, 2.0f};     // the built-in plane, 20 m across
constexpr Vec3 kCameraPosition{0.0f, 4.0f, 8.0f};  // meters: behind and above the middle
constexpr Vec3 kCameraTarget{0.0f, 0.0f, 0.0f};    // the middle of the ground

bool scene_name_character(char c) {
  const auto letter = static_cast<unsigned char>(c);
  return std::islower(letter) || std::isdigit(letter) || c == '_' || c == '-';
}

bool starts_with_letter(const std::string& name) {
  return !name.empty() && std::islower(static_cast<unsigned char>(name.front()));
}

bool is_main_scene(const std::filesystem::path& game_dir, const std::filesystem::path& file) {
  return file.lexically_normal() == (game_dir / config::kSceneFileName).lexically_normal();
}

// A node of the document moved to `position` and turned by `rotation`.
void place(SceneDocument& document, int node, Vec3 position, Quat rotation) {
  Transform local = document.node(node).local;
  local.position = position;
  local.rotation = rotation;
  document.set_local(node, local, false);
}

void add_ground(SceneDocument& document) {
  const int ground = document.add_primitive(PrimitiveShape::Plane, kGroundName, new_objects::kGrassColor);
  Transform local = document.node(ground).local;
  local.scale = kGroundScale;
  document.set_local(ground, local, false);
}

}  // namespace

bool valid_scene_name(const std::string& name) {
  if (!starts_with_letter(name)) return false;
  if (name == config::kMainSceneName) return false;  // scene.glb's name
  return std::all_of(name.begin(), name.end(), scene_name_character);
}

std::string scene_name_of(const std::filesystem::path& game_dir, const std::filesystem::path& file) {
  return is_main_scene(game_dir, file) ? config::kMainSceneName : file.stem().string();
}

std::optional<std::filesystem::path> new_scene_file(const std::filesystem::path& game_dir, const std::string& name,
                                                    std::string& error) {
  if (!valid_scene_name(name)) {
    error = "use lowercase letters, digits, _ and -, starting with a letter, and not main (e.g. level2)";
    return std::nullopt;
  }
  std::filesystem::path file = scene_file(game_dir, name);
  std::error_code fs_error;
  if (!std::filesystem::exists(file, fs_error)) return file;
  error = "the game already has a scene called " + name;
  return std::nullopt;
}

SceneDocument starting_scene(const std::filesystem::path& file) {
  SceneDocument document = SceneDocument::create_empty(file);
  add_ground(document);
  const int sun = document.add_light(LightType::Directional, new_objects::kSunName, new_objects::kLightColor,
                                     new_objects::kSunIntensity);
  place(document, sun, Vec3::zero(), new_objects::sun_rotation());
  const int camera = document.add_camera(new_objects::kCameraName, new_objects::kCameraFov);
  const Vec3 view = normalize(kCameraTarget - kCameraPosition);
  place(document, camera, kCameraPosition, quat_look_rotation(view, Vec3::up()));
  return document;
}

std::optional<std::filesystem::path> ready_new_scene_file(const std::filesystem::path& game_dir,
                                                          const std::string& name, std::string& error) {
  auto file = new_scene_file(game_dir, name, error);
  if (!file) return std::nullopt;
  std::error_code fs_error;
  std::filesystem::create_directories(file->parent_path(), fs_error);
  if (!fs_error) return file;
  error = "cannot make " + file->parent_path().string() + ": " + fs_error.message();
  return std::nullopt;
}

std::optional<std::filesystem::path> create_scene(const std::filesystem::path& game_dir, const std::string& name,
                                                  std::string& error) {
  auto file = ready_new_scene_file(game_dir, name, error);
  if (!file) return std::nullopt;
  SceneDocument document = starting_scene(*file);
  if (!document.save(error)) return std::nullopt;
  return file;
}

}  // namespace redgem
