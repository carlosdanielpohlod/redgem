#include <doctest/doctest.h>

#include <filesystem>
#include <limits>

#include "app/json_file.hpp"
#include "core/files.hpp"
#include "editor/editor_preferences.hpp"
#include "editor/gizmo.hpp"
#include "scene/scene_ids.hpp"

using namespace redgem;

namespace {

struct Scratch {
  std::filesystem::path folder = std::filesystem::temp_directory_path() / ("redgem-editor-prefs-" + make_scene_id());
  std::filesystem::path file = folder / "editor-preferences.json";

  ~Scratch() {
    std::error_code problem;
    std::filesystem::remove_all(folder, problem);
  }
};

constexpr float kFly = 12.0f;
constexpr float kMove = 2.0f;
constexpr float kRotate = 30.0f;
constexpr float kScale = 0.25f;

}  // namespace

TEST_CASE("editor preferences preserve unrelated values and apply only the changed fields") {
  const Scratch scratch;
  const EditorPreferences defaults;
  std::string error;
  REQUIRE(write_json_file(scratch.file, {{"unknown", true}}, error));
  REQUIRE(write_editor_preferences(scratch.file, defaults, {kFly, kMove, kRotate, kScale}, error));
  const auto loaded = read_editor_preferences(scratch.file);
  CHECK(loaded.fly_speed == kFly);
  CHECK(snap_step(GizmoMode::Translate, loaded) == kMove);
  CHECK(snap_step(GizmoMode::Rotate, loaded) == kRotate);
  CHECK(snap_step(GizmoMode::Scale, loaded) == kScale);

  auto stale = defaults;
  stale.scale_snap = kScale;
  REQUIRE(write_editor_preferences(scratch.file, defaults, stale, error));
  CHECK(read_editor_preferences(scratch.file).fly_speed == kFly);
  CHECK(read_json_file(scratch.file).at("unknown").get<bool>());
}

TEST_CASE("editor preferences fall back from invalid files and fields") {
  const Scratch scratch;
  const EditorPreferences defaults;
  CHECK(read_editor_preferences(scratch.file).fly_speed == defaults.fly_speed);
  std::filesystem::create_directories(scratch.folder);
  std::string error;
  for (const char* broken : {"[]", "42", "{\"fly_speed\":"}) {
    REQUIRE(write_text_file(scratch.file, broken, error));
    CHECK(read_editor_preferences(scratch.file).fly_speed == defaults.fly_speed);
  }
  REQUIRE(write_text_file(scratch.file, R"({"fly_speed":0,"translate_snap":-2,"scale_snap":0.25})", error));
  const auto partial = read_editor_preferences(scratch.file);
  CHECK(partial.fly_speed == defaults.fly_speed);
  CHECK(partial.translate_snap == defaults.translate_snap);
  CHECK(partial.rotate_snap == defaults.rotate_snap);
  CHECK(partial.scale_snap == kScale);
}

TEST_CASE("invalid editor preferences are refused before writing") {
  const Scratch scratch;
  const EditorPreferences defaults;
  std::string error;
  auto invalid = defaults;
  for (const float value : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN()}) {
    invalid.fly_speed = value;
    CHECK_FALSE(write_editor_preferences(scratch.file, defaults, invalid, error));
    CHECK_FALSE(error.empty());
    CHECK_FALSE(std::filesystem::exists(scratch.file));
  }
}

TEST_CASE("editor interface scale can be persisted and cleared independently of camera settings") {
  constexpr float kManualScale = 1.5f;
  const Scratch scratch;
  const EditorPreferences defaults;
  auto manual = defaults;
  manual.interface_scale = kManualScale;
  std::string error;
  REQUIRE(write_editor_preferences(scratch.file, defaults, manual, error));
  auto camera = defaults;
  camera.fly_speed = kFly;
  REQUIRE(write_editor_preferences(scratch.file, defaults, camera, error));
  CHECK(read_editor_preferences(scratch.file).interface_scale == kManualScale);
  REQUIRE(write_editor_preferences(scratch.file, manual, defaults, error));
  CHECK_FALSE(read_editor_preferences(scratch.file).interface_scale.has_value());
  CHECK(read_editor_preferences(scratch.file).fly_speed == kFly);
  CHECK_FALSE(read_json_file(scratch.file).contains("interface_scale"));
}

TEST_CASE("an out of range interface scale falls back to automatic and cannot be saved") {
  const Scratch scratch;
  std::filesystem::create_directories(scratch.folder);
  const EditorPreferences defaults;
  auto invalid = defaults;
  invalid.interface_scale = 0.0f;
  std::string error;
  REQUIRE(write_text_file(scratch.file, R"({"interface_scale":0})", error));
  CHECK_FALSE(read_editor_preferences(scratch.file).interface_scale.has_value());
  CHECK_FALSE(write_editor_preferences(scratch.file, defaults, invalid, error));
}

TEST_CASE("an editor preferences save preserves a damaged file and refuses a directory") {
  const Scratch scratch;
  const EditorPreferences defaults;
  std::string error;
  std::filesystem::create_directories(scratch.folder);
  const std::string broken = "{";
  REQUIRE(write_text_file(scratch.file, broken, error));
  REQUIRE(write_editor_preferences(scratch.file, defaults, defaults, error));
  CHECK(read_text_file(scratch.file.string() + ".damaged", error) == broken);
  const auto directory = scratch.folder / "directory";
  std::filesystem::create_directory(directory);
  CHECK_FALSE(write_editor_preferences(directory, defaults, defaults, error));
  CHECK(std::filesystem::is_directory(directory));
}

TEST_CASE("editor fly keys swap conflicts, persist, and fall back from duplicate keys") {
  const Scratch scratch;
  const EditorPreferences defaults;
  auto edited = defaults;
  rebind_fly_key(edited.fly_bindings, &FlyBindings::forward, Key::A);
  CHECK(edited.fly_bindings.forward == Key::A);
  CHECK(edited.fly_bindings.left == Key::W);
  CHECK(valid_fly_bindings(edited.fly_bindings));

  std::string error;
  REQUIRE(write_editor_preferences(scratch.file, defaults, edited, error));
  const auto saved = read_editor_preferences(scratch.file);
  CHECK(same_fly_bindings(saved.fly_bindings, edited.fly_bindings));

  REQUIRE(write_text_file(scratch.file, R"({"fly_keys":{"forward":"a","left":"a"}})", error));
  CHECK(same_fly_bindings(read_editor_preferences(scratch.file).fly_bindings, defaults.fly_bindings));
}
