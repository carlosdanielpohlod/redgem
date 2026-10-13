#include <doctest/doctest.h>

#include <string>

#include "editor/ui/hierarchy_rows.hpp"
#include "editor/ui/style.hpp"
#include "scene/props.hpp"

using namespace redgem;
using namespace redgem::ui::hierarchy;

namespace {

NodeDesc named(const std::string& name) {
  NodeDesc node;
  node.name = name;
  return node;
}

}  // namespace

TEST_CASE("a row shows its object's name, and what kind of object it is") {
  NodeDesc empty = named("Spawn");
  CHECK(label_of(empty).name == "Spawn");
  CHECK(std::string(label_of(empty).icon) == ICON_FA_CIRCLE_DOT);
  NodeDesc group = named("Level");
  group.children = {1};
  CHECK(std::string(label_of(group).icon) == ICON_FA_LAYER_GROUP);
  NodeDesc crate = named("Crate");
  crate.mesh = 0;
  CHECK(std::string(label_of(crate).icon) == ICON_FA_CUBE);
  NodeDesc camera = named("Camera");
  camera.camera = CameraDesc{};
  CHECK(std::string(label_of(camera).icon) == ICON_FA_VIDEO);
  NodeDesc sun = named("Sun");
  sun.light = LightDesc{};
  sun.light->type = LightType::Directional;
  CHECK(std::string(label_of(sun).icon) == ICON_FA_SUN);
  sun.light->type = LightType::Point;
  CHECK(std::string(label_of(sun).icon) == ICON_FA_LIGHTBULB);
}

TEST_CASE("templates, hidden objects and objects with scripts are marked on their row") {
  NodeDesc plain = named("Crate");
  CHECK_FALSE(label_of(plain).dimmed());
  CHECK_FALSE(label_of(plain).has_script);
  NodeDesc hidden = plain;
  hidden.props[props::kVisible] = false;
  CHECK(label_of(hidden).hidden);
  CHECK(label_of(hidden).dimmed());
  NodeDesc prefab = plain;
  prefab.mesh = 0;
  prefab.props[props::kPrefab] = true;
  CHECK(label_of(prefab).template_object);
  CHECK(label_of(prefab).dimmed());
  CHECK(std::string(label_of(prefab).icon) == ICON_FA_CUBES);
  NodeDesc scripted = plain;
  scripted.props[props::kScript] = std::string("enemy.rb");
  CHECK(label_of(scripted).has_script);
}
