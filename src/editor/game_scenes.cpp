#include "editor/game_scenes.hpp"

#include <algorithm>
#include <cctype>

#include "app/scene_files.hpp"
#include "core/config.hpp"

namespace redgem {
namespace {

// What a new scene starts with: like the scene of a new game.
constexpr const char* kGroundName = "Ground";
constexpr const char* kSunName = "Sun";
constexpr const char* kCameraName = "Camera";
constexpr Color kGroundColor{0.33f, 0.46f, 0.25f, 1.0f};  // grass
constexpr Vec3 kGroundScale{2.0f, 1.0f, 2.0f};            // the built-in plane, 20 m across
constexpr Color kSunColor{1.0f, 1.0f, 1.0f, 1.0f};        // white
constexpr float kSunIntensity = 3.0f;                     // W/m2, like Blender's sun strength
constexpr Vec3 kSunDirection{-0.4f, -1.0f, -0.6f};        // down at an angle
constexpr float kCameraFov = 0.87f;                       // ~50 degrees
constexpr Vec3 kCameraPosition{0.0f, 4.0f, 8.0f};         // meters: behind and above the middle
constexpr Vec3 kCameraTarget{0.0f, 0.0f, 0.0f};           // the middle of the ground

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

// A node of the document moved to `position` and turned to look along `direction`.
void place(SceneDocument& document, int node, Vec3 position, Vec3 direction) {
  Transform local = document.node(node).local;
  local.position = position;
  local.rotation = quat_look_rotation(normalize(direction), Vec3::up());
  document.set_local(node, local, false);
}

void add_ground(SceneDocument& document) {
  const int ground = document.add_primitive(PrimitiveShape::Plane, kGroundName, kGroundColor);
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
  const int sun = document.add_light(LightType::Directional, kSunName, kSunColor, kSunIntensity);
  place(document, sun, Vec3::zero(), kSunDirection);
  const int camera = document.add_camera(kCameraName, kCameraFov);
  place(document, camera, kCameraPosition, kCameraTarget - kCameraPosition);
  return document;
}

std::optional<std::filesystem::path> create_scene(const std::filesystem::path& game_dir, const std::string& name,
                                                  std::string& error) {
  auto file = new_scene_file(game_dir, name, error);
  if (!file) return std::nullopt;
  std::error_code fs_error;
  std::filesystem::create_directories(file->parent_path(), fs_error);
  SceneDocument document = starting_scene(*file);
  if (!document.save(error)) return std::nullopt;
  return file;
}

}  // namespace redgem
