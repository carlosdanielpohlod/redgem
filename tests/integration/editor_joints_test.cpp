// The Joint component as the editor makes it: added and undone, saved and loaded, copied with the
// object it holds to, and played.

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "editor/editor_session.hpp"
#include "input/input_source.hpp"
#include "physics/joint_config.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/props.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr int kSwingFrames = 180;
constexpr float kTolerance = 0.05f;
constexpr double kLimit = 45.0;  // degrees the lid may fall
constexpr Vec3 kLidSpot{0.0f, 4.0f, 0.0f};

std::filesystem::path copy_sandbox(const std::string& name) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("redgem_joints_" + name);
  std::filesystem::remove_all(dir);
  std::filesystem::copy(fixture("games/sandbox"), dir, std::filesystem::copy_options::recursive);
  return dir;
}

std::unique_ptr<EditorSession> open(const std::filesystem::path& dir) {
  auto opened = EditorSession::open(dir);
  if (auto* error = std::get_if<std::string>(&opened)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<EditorSession>>(opened));
}

const PropertyMap& props_of(EditorSession& session, ObjectId id) { return session.document().node(id).props; }

// A dynamic cube with a hinge to the world along its left edge, on an axis along z, that lets its
// right side fall `kLimit` degrees: as the Inspector's Joint card sets it up.
ObjectId hinged_lid(EditorSession& session) {
  const ObjectId lid = session.create_primitive(PrimitiveShape::Cube, kLidSpot);
  SceneDocument& doc = session.document();
  doc.set_prop(lid, props::kBody, PropertyValue(std::string(props::kBodyDynamic)));
  session.add_joint(lid, JointKind::Hinge);
  doc.set_prop(lid, props::kJointAnchor[0], -0.5);
  doc.set_prop(lid, props::kJointAxis[1], 0.0);
  doc.set_prop(lid, props::kJointAxis[2], 1.0);
  doc.set_prop(lid, props::kJointLow, -kLimit);
  doc.set_prop(lid, props::kJointHigh, 0.0);
  session.refresh_preview();
  return lid;
}

}  // namespace

TEST_CASE("a joint added in the editor is one undo step, and undo and redo take it away and back") {
  const auto dir = copy_sandbox("undo");
  auto session = open(dir);
  const ObjectId cube = session->create_primitive(PrimitiveShape::Cube, kLidSpot);
  session->add_joint(cube, JointKind::Slider);
  CHECK(get_string(props_of(*session, cube), props::kJoint) == std::optional<std::string>(props::kJointSlider));
  session->undo();
  CHECK_FALSE(joint_config_from_props(props_of(*session, cube)).has_value());
  session->redo();
  CHECK(joint_config_from_props(props_of(*session, cube))->kind == JointKind::Slider);

  const ObjectId crate = session->scene().find("Crate");
  session->set_joint_partner(cube, crate);
  CHECK(get_string(props_of(*session, cube), props::kJointTo) == session->document().node(crate).stable_id);
  // Another kind keeps the other end and anchor, and drops what the old kind alone took.
  session->set_joint_kind(cube, JointKind::Hinge);
  session->document().set_prop(cube, props::kJointLow, -10.0);
  session->document().set_prop(cube, props::kJointHigh, 10.0);
  session->set_joint_kind(cube, JointKind::Slider);
  CHECK(props_of(*session, cube).count(props::kJointLow) == 0);
  CHECK(get_string(props_of(*session, cube), props::kJointTo) == session->document().node(crate).stable_id);
  session->undo();
  CHECK(props_of(*session, cube).count(props::kJointLow) == 1);
  session->remove_joint(cube);
  for (const std::string& key : joint_property_keys()) CHECK(props_of(*session, cube).count(key) == 0);
  session->undo();  // the whole component comes back at once
  CHECK(get_string(props_of(*session, cube), props::kJointTo) == session->document().node(crate).stable_id);
  session.reset();
  std::filesystem::remove_all(dir);
}

TEST_CASE("a scene with a hinge is saved and loaded with it") {
  const auto dir = copy_sandbox("save");
  auto session = open(dir);
  const ObjectId lid = hinged_lid(*session);
  const ObjectId crate = session->scene().find("Crate");
  session->set_joint_partner(lid, crate);
  std::string error;
  REQUIRE_MESSAGE(session->save(error), error);

  const LoadResult loaded = load_gltf_scene((dir / config::kSceneFileName).string());
  REQUIRE(loaded.ok);
  const int saved = loaded.scene.find_node(session->document().node(lid).name);
  REQUIRE(saved != kNoIndex);
  const auto joint = joint_config_from_props(loaded.scene.nodes[saved].props);
  REQUIRE(joint.has_value());
  CHECK(joint->kind == JointKind::Hinge);
  CHECK(joint->other == loaded.scene.nodes[loaded.scene.find_node("Crate")].stable_id);
  CHECK(approx_equal(joint->axis, Vec3{0, 0, 1}, redgem::test::kTolerance));
  REQUIRE(joint->limits.has_value());
  CHECK(joint->limits->first == doctest::Approx(-kLimit));
  session.reset();
  std::filesystem::remove_all(dir);
}

TEST_CASE("an object duplicated with the one its joint holds to is held to the copy") {
  const auto dir = copy_sandbox("duplicate");
  auto session = open(dir);
  const ObjectId frame = session->create_empty(kLidSpot);
  const ObjectId door = session->create_primitive(PrimitiveShape::Cube, kLidSpot);
  session->reparent(door, frame);
  session->add_joint(door, JointKind::Hinge);
  session->set_joint_partner(door, frame);
  const ObjectId outside = session->create_primitive(PrimitiveShape::Cube, kLidSpot);
  session->add_joint(outside, JointKind::Fixed);
  session->set_joint_partner(outside, frame);

  const int copy = session->document().duplicate(std::vector<int>{frame, outside}).front();
  session->refresh_preview();
  const NodeDesc& frame_copy = session->document().node(copy);
  const NodeDesc& door_copy = session->document().node(frame_copy.children.front());
  CHECK(get_string(door_copy.props, props::kJointTo) == frame_copy.stable_id);
  const int outside_copy = session->document().node_count() - 1;
  CHECK(get_string(session->document().node(outside_copy).props, props::kJointTo) == frame_copy.stable_id);
  session.reset();
  std::filesystem::remove_all(dir);
}

TEST_CASE("a hinge set up in the editor swings within its limits when the game plays") {
  const auto dir = copy_sandbox("play");
  auto session = open(dir);
  const ObjectId lid = hinged_lid(*session);
  const std::string name = session->document().node(lid).name;
  ScriptedInputSource input;
  std::string error;
  REQUIRE_MESSAGE(session->play({input}, error), error);
  for (int i = 0; i < kSwingFrames; ++i) session->tick(config::kFixedDeltaSeconds);
  const ObjectId playing = session->scene().find(name);
  REQUIRE(playing != kNoObject);
  // A unit cube a meter wide: its middle half a meter out from the hinge, turned down by the limit.
  const float drop = std::sin(degrees_to_radians(static_cast<float>(kLimit))) * 0.5f;
  CHECK(session->scene().world_transform(playing).position.y ==
        doctest::Approx(kLidSpot.y - drop).epsilon(kTolerance));
  session->stop();
  CHECK(session->scene().world_transform(lid).position.y == doctest::Approx(kLidSpot.y));
  session.reset();
  std::filesystem::remove_all(dir);
}
