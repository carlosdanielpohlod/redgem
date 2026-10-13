#include <doctest/doctest.h>

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
