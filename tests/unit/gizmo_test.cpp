#include <doctest/doctest.h>

#include "core/files.hpp"
#include "editor/editor_config.hpp"
#include "editor/gizmo.hpp"

using namespace redgem;

TEST_CASE("snapping moves by half meters, turns by a few degrees and scales by tenths") {
  CHECK(snap_step(GizmoMode::Translate) == editor_config::kTranslateSnap);
  CHECK(snap_step(GizmoMode::Rotate) == editor_config::kRotateSnapDegrees);
  CHECK(snap_step(GizmoMode::Scale) == editor_config::kScaleSnap);
}

TEST_CASE("a gizmo starts out moving things") {
  const Gizmo gizmo;
  CHECK(gizmo.mode() == GizmoMode::Translate);
}

TEST_CASE("camera speed and snap preferences persist with per-field defaults") {
  const auto folder = std::filesystem::temp_directory_path() / "redgem-editor-motion-preferences";
  const auto file = folder / "editor-preferences.json";
  std::filesystem::remove_all(folder);
  const EditorPreferences defaults;
  CHECK(read_editor_preferences(file).fly_speed == defaults.fly_speed);

  constexpr float kFly = 12.0f;
  constexpr float kMove = 2.0f;
  constexpr float kRotate = 30.0f;
  constexpr float kScale = 0.25f;
  std::string error;
  REQUIRE_MESSAGE(write_editor_preferences(file, {kFly, kMove, kRotate, kScale}, error), error);
  const auto loaded = read_editor_preferences(file);
  CHECK(loaded.fly_speed == kFly);
  CHECK(snap_step(GizmoMode::Translate, loaded) == kMove);
  CHECK(snap_step(GizmoMode::Rotate, loaded) == kRotate);
  CHECK(snap_step(GizmoMode::Scale, loaded) == kScale);

  REQUIRE(write_text_file(file, R"({"fly_speed":"fast","rotate_snap":500,"scale_snap":0.25})", error));
  const auto partial = read_editor_preferences(file);
  CHECK(partial.fly_speed == defaults.fly_speed);
  CHECK(partial.rotate_snap == defaults.rotate_snap);
  CHECK(partial.scale_snap == kScale);
  std::filesystem::remove_all(folder);
}
