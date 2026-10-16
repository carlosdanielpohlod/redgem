#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <variant>

#include <nlohmann/json.hpp>

#include "editor/document/glb_file.hpp"
#include "editor/document/scene_document.hpp"
#include "scene/shapes/primitives.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;
using redgem::test::kTolerance;

namespace {

constexpr Vec3 kMovedPosition{4, 5, 6};
constexpr double kEditedSpeed = 9.5;
constexpr double kFixtureSpeed = 2.5;  // tools/blender/build_test_fixtures.py
constexpr std::size_t kGlbHeaderBytes = 12;
constexpr std::size_t kGlbChunkHeaderBytes = 8;
constexpr std::size_t kGlbAlignment = 4;
constexpr std::size_t kLengthOffset = 8;

std::uint32_t u32_at(const std::vector<char>& bytes, std::size_t offset) {
  std::uint32_t value = 0;
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

SceneDocument open_fixture() {
  auto opened = SceneDocument::open(fixture("hierarchy.glb"));
  if (auto* error = std::get_if<std::string>(&opened)) FAIL(*error);
  return std::move(std::get<SceneDocument>(opened));
}

std::filesystem::path temp_copy_path(const std::string& name) {
  return std::filesystem::temp_directory_path() / ("redgem_test_" + name + ".glb");
}

}  // namespace

TEST_CASE("documents open clean and edits mark them dirty") {
  SceneDocument doc = open_fixture();
  CHECK_FALSE(doc.dirty());
  CHECK_FALSE(doc.can_undo());
  const int root = doc.desc().find_node("Root");
  doc.set_name(root, "Base");
  CHECK(doc.dirty());
  CHECK(doc.node(root).name == "Base");
}

TEST_CASE("undo and redo restore every kind of edit") {
  SceneDocument doc = open_fixture();
  const int root = doc.desc().find_node("Root");
  const Transform before = doc.node(root).local;
  Transform moved = before;
  moved.position = kMovedPosition;
  doc.set_local(root, moved);
  doc.set_prop(root, "speed", kEditedSpeed);
  doc.remove_prop(root, "label");

  CHECK_FALSE(get_string(doc.node(root).props, "label").has_value());
  REQUIRE(doc.undo());
  CHECK(get_string(doc.node(root).props, "label") == "hello");
  REQUIRE(doc.undo());
  CHECK(get_number(doc.node(root).props, "speed") == doctest::Approx(kFixtureSpeed));
  REQUIRE(doc.undo());
  CHECK(approx_equal(doc.node(root).local.position, before.position, kTolerance));
  CHECK_FALSE(doc.undo());

  REQUIRE(doc.redo());
  CHECK(approx_equal(doc.node(root).local.position, kMovedPosition, kTolerance));
  doc.set_name(root, "Other");  // a new edit clears the redo history
  CHECK_FALSE(doc.can_redo());
}

TEST_CASE("unrecorded transform updates share one undo step (gizmo drags)") {
  SceneDocument doc = open_fixture();
  const int root = doc.desc().find_node("Root");
  const Vec3 start = doc.node(root).local.position;
  doc.checkpoint();
  for (const float x : {1.0f, 2.0f, 3.0f}) {
    Transform t = doc.node(root).local;
    t.position.x = x;
    doc.set_local(root, t, false);
  }
  REQUIRE(doc.undo());
  CHECK(approx_equal(doc.node(root).local.position, start, kTolerance));
  CHECK_FALSE(doc.can_undo());
}

TEST_CASE("duplicating copies the subtree with unique names") {
  SceneDocument doc = open_fixture();
  const int root = doc.desc().find_node("Root");
  const int count = doc.node_count();
  const int copy = doc.duplicate(root);
  CHECK(doc.node_count() == count + 2);  // Root and its Child
  CHECK(doc.node(copy).name == "Root.001");
  REQUIRE(doc.node(copy).children.size() == 1);
  const NodeDesc& child = doc.node(doc.node(copy).children[0]);
  CHECK(child.name == "Child.001");
  CHECK(child.parent == copy);
  CHECK(child.mesh == doc.node(doc.desc().find_node("Child")).mesh);  // shares the mesh
  CHECK(doc.unique_name("Root") == "Root.002");
}

TEST_CASE("removing a node removes its children and remaps indices") {
  SceneDocument doc = open_fixture();
  doc.remove(doc.desc().find_node("Root"));
  CHECK(doc.desc().find_node("Root") == kNoIndex);
  CHECK(doc.desc().find_node("Child") == kNoIndex);
  for (int i = 0; i < doc.node_count(); ++i) {
    const NodeDesc& node = doc.node(i);
    if (node.parent != kNoIndex) {
      REQUIRE(node.parent < doc.node_count());
      const auto& siblings = doc.node(node.parent).children;
      CHECK(std::find(siblings.begin(), siblings.end(), i) != siblings.end());
    }
    for (const int child : node.children) {
      REQUIRE(child < doc.node_count());
      CHECK(doc.node(child).parent == i);
    }
  }
  REQUIRE(doc.undo());
  CHECK(doc.desc().find_node("Child") != kNoIndex);
}

TEST_CASE("saving writes the edits back into a .glb the loader understands") {
  SceneDocument doc = open_fixture();
  const int root = doc.desc().find_node("Root");
  Transform moved = doc.node(root).local;
  moved.position = kMovedPosition;
  doc.set_local(root, moved);
  doc.set_prop(root, "speed", kEditedSpeed);
  doc.set_prop(root, "boss", true);
  doc.duplicate(root);

  const std::filesystem::path out = temp_copy_path("save");
  std::string error;
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  CHECK_FALSE(doc.dirty());

  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  const SceneDesc& scene = loaded.scene;
  const int saved_root = scene.find_node("Root");
  REQUIRE(saved_root != kNoIndex);
  CHECK(approx_equal(scene.nodes[saved_root].local.position, kMovedPosition, kTolerance));
  CHECK(get_number(scene.nodes[saved_root].props, "speed") == doctest::Approx(kEditedSpeed));
  CHECK(get_bool(scene.nodes[saved_root].props, "boss") == true);
  CHECK(scene.find_node("Root.001") != kNoIndex);
  CHECK(scene.find_node("Child.001") != kNoIndex);
  // Geometry, camera and light survive untouched.
  const NodeDesc& child = scene.nodes[scene.find_node("Child")];
  REQUIRE(child.mesh != kNoIndex);
  CHECK_FALSE(scene.meshes[child.mesh].primitives.empty());
  CHECK(scene.nodes[scene.find_node("Camera")].camera.has_value());
  CHECK(scene.nodes[scene.find_node("Sun")].light.has_value());
  std::filesystem::remove(out);
}

TEST_CASE("opening something that is not a scene fails cleanly") {
  auto opened = SceneDocument::open(fixture("does_not_exist.glb"));
  CHECK(std::holds_alternative<std::string>(opened));
}

TEST_CASE("undoing back to the saved state makes the document clean again") {
  SceneDocument doc = open_fixture();
  const int root = doc.desc().find_node("Root");
  doc.set_name(root, "Base");
  REQUIRE(doc.dirty());
  doc.undo();
  CHECK_FALSE(doc.dirty());
  doc.redo();
  CHECK(doc.dirty());
}

TEST_CASE("a drag that changes nothing leaves no undo step") {
  SceneDocument doc = open_fixture();
  doc.checkpoint();                    // the gizmo or a field was grabbed...
  doc.drop_checkpoint_if_unchanged();  // ...and released without moving
  CHECK_FALSE(doc.can_undo());
  CHECK_FALSE(doc.dirty());
  doc.checkpoint();
  const int root = doc.desc().find_node("Root");
  Transform moved = doc.node(root).local;
  moved.position = kMovedPosition;
  doc.set_local(root, moved, false);
  doc.drop_checkpoint_if_unchanged();
  CHECK(doc.can_undo());
}

TEST_CASE("saving keeps glTF data the editor does not edit") {
  // Add fields the editor does not understand to a copy of the fixture.
  std::string error;
  std::optional<GlbFile> glb = read_glb(fixture("hierarchy.glb"), error);
  REQUIRE(glb);
  for (auto& node : glb->json["nodes"]) {
    if (node["name"] == "Root") {
      node["extensions"]["EXT_custom"] = {{"answer", 42}};
      node["extras"]["waypoints"] = {1, 2, 3};
    }
  }
  const std::filesystem::path source = temp_copy_path("unknown_fields_in");
  REQUIRE(write_glb(source, *glb, error));

  auto opened = SceneDocument::open(source);
  REQUIRE(std::holds_alternative<SceneDocument>(opened));
  SceneDocument doc = std::move(std::get<SceneDocument>(opened));
  const int root = doc.desc().find_node("Root");
  doc.remove_prop(root, "label");
  const std::filesystem::path out = temp_copy_path("unknown_fields_out");
  REQUIRE_MESSAGE(doc.save_as(out, error), error);

  std::optional<GlbFile> saved = read_glb(out, error);
  REQUIRE(saved);
  bool found = false;
  for (const auto& node : saved->json["nodes"]) {
    if (node["name"] != "Root") continue;
    found = true;
    CHECK(node["extensions"]["EXT_custom"]["answer"] == 42);
    CHECK(node["extras"]["waypoints"].size() == 3);
    CHECK_FALSE(node["extras"].contains("label"));  // removed in the editor
    CHECK(node["extras"]["script"] == "spinner.rb");
  }
  CHECK(found);
  std::filesystem::remove(source);
  std::filesystem::remove(out);
}

TEST_CASE("written .glb files follow the binary layout") {
  SceneDocument doc = open_fixture();
  const std::filesystem::path out = temp_copy_path("layout");
  std::string error;
  REQUIRE(doc.save_as(out, error));
  std::vector<char> bytes;
  {  // (closed before the file is removed: Windows removes no file that is open)
    std::ifstream in(out, std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }
  REQUIRE(bytes.size() > kGlbHeaderBytes);
  CHECK(u32_at(bytes, kLengthOffset) == bytes.size());
  std::size_t offset = kGlbHeaderBytes;
  while (offset < bytes.size()) {
    const std::uint32_t length = u32_at(bytes, offset);
    CHECK(length % kGlbAlignment == 0);
    offset += kGlbChunkHeaderBytes + length;
  }
  CHECK(offset == bytes.size());
  std::filesystem::remove(out);
}

TEST_CASE("a scene too big for the lengths a .glb is written with is refused, not cut short") {
  constexpr std::uint64_t kLimit = 0xFFFFFFFFull;  // what 32 bits can say
  constexpr std::uint64_t kHeaders = kGlbHeaderBytes + 2 * kGlbChunkHeaderBytes;
  constexpr std::uint64_t kJsonBytes = 1024;
  CHECK(fits_in_glb(kJsonBytes, 0));
  CHECK(fits_in_glb(kJsonBytes, kLimit - kHeaders - kJsonBytes));
  CHECK_FALSE(fits_in_glb(kJsonBytes, kLimit - kHeaders - kJsonBytes + kGlbAlignment));
  CHECK_FALSE(fits_in_glb(kJsonBytes, kLimit + 1));  // would be written as a length of nothing
  CHECK_FALSE(fits_in_glb(kLimit + 1, 0));
  CHECK_FALSE(fits_in_glb(kLimit, kLimit));
  // A file with no binary chunk has no header for one.
  CHECK(fits_in_glb(kLimit - kGlbHeaderBytes - kGlbChunkHeaderBytes, 0));
  CHECK_FALSE(fits_in_glb(kLimit - kGlbHeaderBytes - kGlbChunkHeaderBytes + 1, 0));
}

TEST_CASE("an empty scene cannot be saved") {
  SceneDocument doc = open_fixture();
  while (doc.node_count() > 0) doc.remove(0);
  std::string error;
  CHECK_FALSE(doc.save_as(temp_copy_path("empty"), error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("objects created in the editor are saved as real glTF") {
  constexpr Color kRed{0.8f, 0.1f, 0.1f, 1.0f};
  constexpr Color kWarm{1.0f, 0.8f, 0.6f, 1.0f};
  constexpr float kLightIntensity = 300.0f;
  constexpr float kFov = 0.9f;
  SceneDocument doc = open_fixture();
  const int meshes_before = static_cast<int>(doc.desc().meshes.size());
  const int box = doc.add_primitive(PrimitiveShape::Cube, "Box", kRed);
  const int ball = doc.add_primitive(PrimitiveShape::Sphere, "Ball", kRed, box);
  const int lamp = doc.add_light(LightType::Point, "Lamp", kWarm, kLightIntensity);
  const int eye = doc.add_camera("Eye", kFov);
  const int pivot = doc.add_empty("Pivot");
  CHECK(doc.node(ball).parent == box);
  CHECK(doc.desc().meshes.size() == static_cast<std::size_t>(meshes_before + 2));
  CHECK(get_string(doc.node(box).props, "collider") == "box");
  CHECK(get_string(doc.node(ball).props, "collider") == "sphere");
  CHECK(doc.material_of(box) != kNoIndex);
  CHECK(doc.material_users(doc.material_of(box)) == 1);
  CHECK(doc.node(pivot).mesh == kNoIndex);
  CHECK(doc.unique_name("Box") == "Box.001");

  const std::filesystem::path out = temp_copy_path("created");
  std::string error;
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  const SceneDesc& scene = loaded.scene;
  const NodeDesc& saved_box = scene.nodes[scene.find_node("Box")];
  REQUIRE(saved_box.mesh != kNoIndex);
  const PrimitiveDesc& primitive = scene.meshes[saved_box.mesh].primitives.front();
  CHECK(primitive.indices.size() == make_primitive(PrimitiveShape::Cube).indices.size());
  CHECK(scene.meshes[saved_box.mesh].bounds.valid);
  CHECK(scene.materials[primitive.material].base_color.r == doctest::Approx(kRed.r));
  REQUIRE(scene.find_node("Ball") != kNoIndex);
  REQUIRE(scene.find_node("Box") != kNoIndex);
  CHECK(scene.nodes[scene.find_node("Ball")].parent == scene.find_node("Box"));
  REQUIRE(scene.nodes[scene.find_node("Lamp")].light.has_value());
  CHECK(scene.nodes[scene.find_node("Lamp")].light->intensity == doctest::Approx(kLightIntensity));
  REQUIRE(scene.nodes[scene.find_node("Eye")].camera.has_value());
  CHECK(scene.nodes[scene.find_node("Eye")].camera->fov_y_radians == doctest::Approx(kFov));
  // The original geometry is still intact next to the new one.
  CHECK_FALSE(scene.meshes[scene.nodes[scene.find_node("Child")].mesh].primitives.empty());
  (void)lamp;
  (void)eye;
  std::filesystem::remove(out);
}

TEST_CASE("material edits are undoable and saved, including bright emission") {
  constexpr Color kBlue{0.1f, 0.2f, 0.9f, 1.0f};
  constexpr Color kGreen{0.1f, 0.9f, 0.2f, 1.0f};
  constexpr float kMetal = 0.9f;
  constexpr Vec3 kGlow{4.0f, 2.0f, 0.0f};  // brighter than glTF's 0..1 factor
  SceneDocument doc = open_fixture();
  const int box = doc.add_primitive(PrimitiveShape::Cube, "Box", kBlue);
  const int material = doc.material_of(box);
  MaterialDesc edited = doc.material(material);
  edited.base_color = kGreen;
  edited.metallic = kMetal;
  edited.emissive = kGlow;
  doc.set_material(material, edited);
  CHECK(doc.material(material).base_color.g == doctest::Approx(kGreen.g));
  REQUIRE(doc.undo());
  CHECK(doc.material(material).base_color.b == doctest::Approx(kBlue.b));
  REQUIRE(doc.redo());

  const std::filesystem::path out = temp_copy_path("material");
  std::string error;
  REQUIRE(doc.save_as(out, error));
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE(loaded.ok);
  const MaterialDesc& saved = loaded.scene.materials[material];
  CHECK(saved.metallic == doctest::Approx(kMetal));
  CHECK(approx_equal(saved.emissive, kGlow, 1e-3f));
  std::filesystem::remove(out);
}

TEST_CASE("an empty document becomes a valid scene once it has objects") {
  constexpr Color kGray{0.5f, 0.5f, 0.5f, 1.0f};
  const std::filesystem::path out = temp_copy_path("from_scratch");
  SceneDocument doc = SceneDocument::create_empty(out);
  CHECK(doc.node_count() == 0);
  CHECK(doc.dirty());
  std::string error;
  CHECK_FALSE(doc.save(error));  // nothing to save yet
  doc.add_primitive(PrimitiveShape::Plane, "Floor", kGray);
  REQUIRE_MESSAGE(doc.save(error), error);
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  CHECK(loaded.scene.find_node("Floor") != kNoIndex);
  std::filesystem::remove(out);
}

TEST_CASE("reparenting keeps the world placement and refuses cycles") {
  constexpr Color kGray{0.5f, 0.5f, 0.5f, 1.0f};
  constexpr Vec3 kHolderPosition{10.0f, 0.0f, 0.0f};
  constexpr Vec3 kBoxPosition{2.0f, 1.0f, 0.0f};
  SceneDocument doc = open_fixture();
  const int holder = doc.add_empty("Holder");
  Transform placed = doc.node(holder).local;
  placed.position = kHolderPosition;
  doc.set_local(holder, placed);
  const int box = doc.add_primitive(PrimitiveShape::Cube, "Box", kGray);
  placed = doc.node(box).local;
  placed.position = kBoxPosition;
  doc.set_local(box, placed);

  const int moved = doc.reparent(box, doc.desc().find_node("Holder"));
  const int parent = doc.desc().find_node("Holder");
  CHECK(doc.node(moved).name == "Box");
  CHECK(doc.node(moved).parent == parent);
  CHECK(moved > parent);  // parents stay before children
  CHECK(approx_equal(doc.node(moved).local.position, kBoxPosition - kHolderPosition, kTolerance));
  // A node cannot go under its own child.
  CHECK(doc.reparent(parent, moved) == parent);
  CHECK(doc.node(parent).parent == kNoIndex);

  const int back = doc.reparent(moved, kNoIndex);
  CHECK(doc.node(back).parent == kNoIndex);
  CHECK(approx_equal(doc.node(back).local.position, kBoxPosition, kTolerance));
  CHECK(doc.node(doc.desc().find_node("Holder")).children.empty());
  REQUIRE(doc.undo());
  CHECK(doc.node(doc.desc().find_node("Box")).parent == doc.desc().find_node("Holder"));
}

TEST_CASE("importing a model brings its objects, meshes and materials") {
  constexpr Color kGray{0.5f, 0.5f, 0.5f, 1.0f};
  const std::filesystem::path out = temp_copy_path("imported");
  SceneDocument doc = SceneDocument::create_empty(out);
  doc.add_primitive(PrimitiveShape::Plane, "Floor", kGray);
  std::string error;
  const int root = doc.import_model(fixture("hierarchy.glb"), error);
  REQUIRE_MESSAGE(root != kNoIndex, error);
  CHECK(doc.node(root).name == "hierarchy");
  const int child = doc.desc().find_node("Child");
  REQUIRE(child != kNoIndex);
  CHECK(doc.node(child).mesh != kNoIndex);

  REQUIRE_MESSAGE(doc.save(error), error);
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  const SceneDesc& scene = loaded.scene;
  const int saved_child = scene.find_node("Child");
  REQUIRE(saved_child != kNoIndex);
  const MeshDesc& mesh = scene.meshes[scene.nodes[saved_child].mesh];
  REQUIRE_FALSE(mesh.primitives.empty());
  CHECK_FALSE(mesh.primitives.front().indices.empty());
  CHECK(scene.find_node("Floor") != kNoIndex);
  REQUIRE(scene.find_node("Root") != kNoIndex);
  REQUIRE(scene.find_node("hierarchy") != kNoIndex);
  CHECK(scene.nodes[scene.find_node("Root")].parent == scene.find_node("hierarchy"));

  std::string missing_error;
  CHECK(doc.import_model(fixture("missing.glb"), missing_error) == kNoIndex);
  CHECK_FALSE(missing_error.empty());
  std::filesystem::remove(out);
}

TEST_CASE("light and camera edits are saved, and a duplicated light gets its own settings") {
  constexpr Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};
  constexpr Color kRed{1.0f, 0.0f, 0.0f, 1.0f};
  constexpr float kIntensity = 100.0f;
  constexpr float kBrighter = 900.0f;
  constexpr float kWideFov = 1.2f;
  constexpr float kFov = 0.8f;
  SceneDocument doc = open_fixture();
  const int lamp = doc.add_light(LightType::Point, "Lamp", kWhite, kIntensity);
  const int copy = doc.duplicate(lamp);
  LightDesc red = *doc.node(copy).light;
  red.color = kRed;
  red.intensity = kBrighter;
  doc.set_light(copy, red);
  const int eye = doc.add_camera("Eye", kFov);
  CameraDesc wide = *doc.node(eye).camera;
  wide.fov_y_radians = kWideFov;
  doc.set_camera(eye, wide);
  doc.set_light(doc.desc().find_node("Root"), red);  // no light: ignored
  CHECK_FALSE(doc.node(doc.desc().find_node("Root")).light.has_value());

  const std::filesystem::path out = temp_copy_path("lights");
  std::string error;
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  const SceneDesc& scene = loaded.scene;
  const LightDesc& original = *scene.nodes[scene.find_node("Lamp")].light;
  const LightDesc& edited = *scene.nodes[scene.find_node("Lamp.001")].light;
  CHECK(original.intensity == doctest::Approx(kIntensity));
  CHECK(original.color.g == doctest::Approx(kWhite.g));
  CHECK(edited.intensity == doctest::Approx(kBrighter));
  CHECK(edited.color.g == doctest::Approx(kRed.g));
  CHECK(scene.nodes[scene.find_node("Eye")].camera->fov_y_radians == doctest::Approx(kWideFov));
  std::filesystem::remove(out);
}

TEST_CASE("saving twice after reparenting keeps every node's own data (lights stay on their node)") {
  constexpr Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};
  constexpr float kIntensity = 3.0f;
  SceneDocument doc = open_fixture();
  const int lamp = doc.add_light(LightType::Directional, "Lamp", kWhite, kIntensity);
  doc.reparent(doc.desc().find_node("Root"), lamp);  // Root and Child now come after Lamp
  const std::filesystem::path out = temp_copy_path("save_twice");
  std::string error;
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  doc.reparent(doc.desc().find_node("Root"), kNoIndex);
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  const SceneDesc& scene = loaded.scene;
  CHECK(scene.nodes[scene.find_node("Lamp")].light.has_value());
  CHECK_FALSE(scene.nodes[scene.find_node("Root")].light.has_value());
  CHECK_FALSE(scene.nodes[scene.find_node("Child")].light.has_value());
  CHECK(get_number(scene.nodes[scene.find_node("Root")].props, "speed") == doctest::Approx(kFixtureSpeed));
  const int child_mesh = scene.nodes[scene.find_node("Child")].mesh;
  REQUIRE(child_mesh != kNoIndex);
  CHECK_FALSE(scene.meshes[child_mesh].primitives.empty());
  std::filesystem::remove(out);
}

TEST_CASE("reparenting refuses indices that do not exist") {
  SceneDocument doc = open_fixture();
  const int root = doc.desc().find_node("Root");
  const int count = doc.node_count();
  CHECK(doc.reparent(root, count) == root);
  CHECK(doc.reparent(count, root) == count);
  CHECK(doc.reparent(-2, root) == -2);
  CHECK_FALSE(doc.can_undo());
}

TEST_CASE("several property removals can share one undo step") {
  SceneDocument doc = open_fixture();
  const int root = doc.desc().find_node("Root");
  doc.checkpoint();
  doc.remove_prop(root, "speed", false);
  doc.remove_prop(root, "label", false);
  REQUIRE(doc.undo());
  CHECK(get_number(doc.node(root).props, "speed").has_value());
  CHECK(get_string(doc.node(root).props, "label").has_value());
  CHECK_FALSE(doc.can_undo());
}

TEST_CASE("a model with morph targets imports into a scene that still loads, under a parent") {
  const std::filesystem::path model = temp_copy_path("morph_model");
  std::string error;
  {
    // The fixture plus a morph target on its first mesh (reusing the positions as the target).
    std::optional<GlbFile> glb = read_glb(fixture("hierarchy.glb"), error);
    REQUIRE(glb.has_value());
    nlohmann::json& mesh = glb->json["meshes"][0];
    nlohmann::json& primitive = mesh["primitives"][0];
    primitive["targets"] = nlohmann::json::array({{{"POSITION", primitive["attributes"]["POSITION"]}}});
    mesh["weights"] = nlohmann::json::array({0.0});
    mesh["extras"]["targetNames"] = nlohmann::json::array({"Smile"});
    REQUIRE(write_glb(model, *glb, error));
    REQUIRE_MESSAGE(load_gltf_scene(model.string()).ok, "the test model itself must be valid");
  }
  const std::filesystem::path out = temp_copy_path("morph_scene");
  SceneDocument doc = SceneDocument::create_empty(out);
  const int holder = doc.add_empty("Holder");
  const int imported = doc.import_model(model, error, holder);
  REQUIRE_MESSAGE(imported != kNoIndex, error);
  CHECK(doc.node(imported).parent == holder);
  REQUIRE_MESSAGE(doc.save(error), error);
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  const int saved = loaded.scene.find_node(doc.node(imported).name);
  REQUIRE(saved != kNoIndex);
  REQUIRE(loaded.scene.find_node("Holder") != kNoIndex);
  CHECK(loaded.scene.nodes[saved].parent == loaded.scene.find_node("Holder"));
  CHECK(doc.import_model(model, error, doc.node_count()) == kNoIndex);  // no such parent
  std::filesystem::remove(model);
  std::filesystem::remove(out);
}

TEST_CASE("skeletons and animations survive editing, saving twice and reordering nodes") {
  auto opened = SceneDocument::open(fixture("skinned.glb"));
  REQUIRE(std::holds_alternative<SceneDocument>(opened));
  SceneDocument doc = std::move(std::get<SceneDocument>(opened));
  // Reorder the nodes: the rig goes under a new empty (which moves it to the end of the list).
  const int holder = doc.add_empty("Holder");
  doc.reparent(doc.desc().find_node("Rig"), holder);
  const SceneDesc& desc = doc.desc();
  REQUIRE(desc.skins.size() == 1);
  CHECK(desc.nodes[desc.skins[0].joints[0]].name == "Lower");  // still the right nodes for the preview
  CHECK(desc.nodes[desc.skins[0].joints[1]].name == "Upper");

  const std::filesystem::path out = temp_copy_path("skinned");
  std::string error;
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  const SceneDesc& saved = loaded.scene;
  REQUIRE(saved.skins.size() == 1);
  CHECK(saved.nodes[saved.skins[0].joints[0]].name == "Lower");
  CHECK(saved.nodes[saved.skins[0].joints[1]].name == "Upper");
  CHECK(saved.nodes[saved.find_node("Arm")].skin == 0);
  REQUIRE(saved.animations.size() == 1);
  for (const AnimationChannel& channel : saved.animations[0].channels) {
    const std::string& target = saved.nodes[channel.node].name;
    CHECK((target == "Lower" || target == "Upper"));
  }

  // Undo puts the nodes back where they were; the skeleton follows.
  REQUIRE(doc.undo());
  CHECK(doc.desc().nodes[doc.desc().skins[0].joints[1]].name == "Upper");
  std::filesystem::remove(out);
}

TEST_CASE("deleting a bone saves the mesh without its skeleton instead of a broken file") {
  auto opened = SceneDocument::open(fixture("skinned.glb"));
  SceneDocument doc = std::move(std::get<SceneDocument>(opened));
  doc.remove(doc.desc().find_node("Upper"));
  CHECK(doc.desc().skins[0].joints[1] == kNoIndex);
  const std::filesystem::path out = temp_copy_path("boneless");
  std::string error;
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  CHECK(loaded.scene.skins.empty());
  CHECK(loaded.scene.nodes[loaded.scene.find_node("Arm")].skin == kNoIndex);
  // What is left of the animation only drives bones that still exist.
  for (const AnimationDesc& animation : loaded.scene.animations) {
    for (const AnimationChannel& channel : animation.channels) CHECK(loaded.scene.nodes[channel.node].name == "Lower");
  }
  std::filesystem::remove(out);
}

TEST_CASE("an imported animated model keeps its skeleton and animation") {
  constexpr Color kGray{0.5f, 0.5f, 0.5f, 1.0f};
  const std::filesystem::path out = temp_copy_path("imported_skin");
  SceneDocument doc = SceneDocument::create_empty(out);
  doc.add_primitive(PrimitiveShape::Plane, "Floor", kGray);
  std::string error;
  REQUIRE_MESSAGE(doc.import_model(fixture("skinned.glb"), error) != kNoIndex, error);
  REQUIRE_MESSAGE(doc.import_model(fixture("skinned.glb"), error) != kNoIndex, error);  // twice: two skeletons
  REQUIRE(doc.desc().skins.size() == 2);
  REQUIRE(doc.desc().animations.size() == 2);
  REQUIRE_MESSAGE(doc.save(error), error);
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  const SceneDesc& scene = loaded.scene;
  REQUIRE(scene.skins.size() == 2);
  REQUIRE(scene.animations.size() == 2);
  // Each arm is deformed by its own bones.
  const int first_arm = scene.find_node("Arm");
  const int second_arm = scene.find_node("Arm.001");
  REQUIRE(first_arm != kNoIndex);
  REQUIRE(second_arm != kNoIndex);
  CHECK(scene.nodes[first_arm].skin != scene.nodes[second_arm].skin);
  const SkinDesc& second_skin = scene.skins[scene.nodes[second_arm].skin];
  CHECK(scene.nodes[second_skin.joints[1]].name == "Upper.001");
  std::filesystem::remove(out);
}

TEST_CASE("streamed light and camera edits count as changes: dirty, undoable, kept by the checkpoint") {
  constexpr Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};
  constexpr float kIntensity = 100.0f;
  constexpr float kBrighter = 400.0f;
  constexpr float kFov = 0.8f;
  constexpr float kWider = 1.1f;
  const std::filesystem::path out = temp_copy_path("light_edit");
  SceneDocument doc = open_fixture();
  const int lamp = doc.add_light(LightType::Point, "Lamp", kWhite, kIntensity);
  const int eye = doc.add_camera("Eye", kFov);
  std::string error;
  REQUIRE(doc.save_as(out, error));
  REQUIRE_FALSE(doc.dirty());

  // What the Inspector does while a slider is dragged: checkpoint, unrecorded edits, then drop
  // the checkpoint if nothing changed.
  doc.checkpoint();
  LightDesc light = *doc.node(lamp).light;
  light.intensity = kBrighter;
  light.type = LightType::Spot;
  doc.set_light(lamp, light, false);
  doc.drop_checkpoint_if_unchanged();
  CHECK(doc.dirty());
  REQUIRE(doc.can_undo());

  doc.checkpoint();
  CameraDesc camera = *doc.node(eye).camera;
  camera.fov_y_radians = kWider;
  doc.set_camera(eye, camera, false);
  doc.drop_checkpoint_if_unchanged();
  REQUIRE(doc.undo());
  CHECK(doc.node(eye).camera->fov_y_radians == doctest::Approx(kFov));
  REQUIRE(doc.undo());
  CHECK(doc.node(lamp).light->intensity == doctest::Approx(kIntensity));
  CHECK_FALSE(doc.dirty());
  std::filesystem::remove(out);
}

TEST_CASE("deleting or undoing one animated character leaves the skeletons of the others") {
  constexpr Color kGray{0.5f, 0.5f, 0.5f, 1.0f};
  const std::filesystem::path out = temp_copy_path("two_skins");
  SceneDocument doc = SceneDocument::create_empty(out);
  doc.add_primitive(PrimitiveShape::Plane, "Floor", kGray);
  std::string error;
  REQUIRE(doc.import_model(fixture("skinned.glb"), error) != kNoIndex);
  const int second = doc.import_model(fixture("skinned.glb"), error);
  REQUIRE(second != kNoIndex);
  doc.remove(second);
  REQUIRE_MESSAGE(doc.save(error), error);
  LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  REQUIRE(loaded.scene.skins.size() == 1);
  const int arm = loaded.scene.find_node("Arm");
  REQUIRE(arm != kNoIndex);
  CHECK(loaded.scene.nodes[arm].skin == 0);
  CHECK(loaded.scene.nodes[loaded.scene.skins[0].joints[1]].name == "Upper");

  // Importing a third and undoing it must not cost the first its skeleton either.
  REQUIRE(doc.import_model(fixture("skinned.glb"), error) != kNoIndex);
  REQUIRE(doc.undo());
  REQUIRE_MESSAGE(doc.save(error), error);
  loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  CHECK(loaded.scene.skins.size() == 1);
  CHECK(loaded.scene.nodes[loaded.scene.find_node("Arm")].skin == 0);
  std::filesystem::remove(out);
}

TEST_CASE("a duplicated character has its own skeleton, also after the original is moved in the list") {
  auto opened = SceneDocument::open(fixture("skinned.glb"));
  SceneDocument doc = std::move(std::get<SceneDocument>(opened));
  const int copy = doc.duplicate(doc.desc().find_node("Rig"));
  REQUIRE(copy != kNoIndex);
  const auto joint_names = [](const SceneDesc& scene, const std::string& mesh) {
    const int node = scene.find_node(mesh);
    REQUIRE(node != kNoIndex);
    const int skin = scene.nodes[node].skin;
    REQUIRE(skin != kNoIndex);
    std::vector<std::string> names;
    for (const int joint : scene.skins[skin].joints) names.push_back(joint == kNoIndex ? "?" : scene.nodes[joint].name);
    return names;
  };
  CHECK(joint_names(doc.desc(), "Arm") == std::vector<std::string>{"Lower", "Upper"});
  CHECK(joint_names(doc.desc(), "Arm.001") == std::vector<std::string>{"Lower.001", "Upper.001"});

  // Reparenting moves the original to the end of the node list: each mesh keeps its own bones.
  const int holder = doc.add_empty("Holder");
  doc.reparent(doc.desc().find_node("Rig"), holder);
  CHECK(joint_names(doc.desc(), "Arm") == std::vector<std::string>{"Lower", "Upper"});
  CHECK(joint_names(doc.desc(), "Arm.001") == std::vector<std::string>{"Lower.001", "Upper.001"});

  const std::filesystem::path out = temp_copy_path("duplicated_skin");
  std::string error;
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  CHECK(loaded.scene.skins.size() == 2);
  CHECK(joint_names(loaded.scene, "Arm") == std::vector<std::string>{"Lower", "Upper"});
  CHECK(joint_names(loaded.scene, "Arm.001") == std::vector<std::string>{"Lower.001", "Upper.001"});
  std::filesystem::remove(out);
}

TEST_CASE("a brush stroke on a saved document stays undoable and unsaved once released") {
  // The editor's own order: grab (checkpoint), sculpt every held frame, release (drop the step if
  // nothing changed). The release used to compare only nodes and materials and threw strokes away.
  constexpr TerrainGrid kGrid{40.0f, 20};
  constexpr Color kGrass{0.3f, 0.5f, 0.2f, 1.0f};
  constexpr TerrainBrush kHill{0.0f, 0.0f, 6.0f, 3.0f, SculptMode::Raise};
  const std::filesystem::path path = temp_copy_path("stroke");
  SceneDocument doc = SceneDocument::create_empty(path);
  const int terrain = doc.add_terrain("Terrain", kGrid, kGrass);
  std::string error;
  REQUIRE_MESSAGE(doc.save(error), error);
  REQUIRE_FALSE(doc.dirty());

  const int mesh = doc.node(terrain).mesh;
  const auto height = [&doc, mesh, &kGrid] {
    return terrain_height(doc.desc().meshes[static_cast<std::size_t>(mesh)].primitives.front(), kGrid, 0.0f, 0.0f);
  };

  doc.checkpoint();
  REQUIRE(doc.sculpt_terrain(terrain, kHill));
  doc.drop_checkpoint_if_unchanged();
  CHECK(doc.dirty());
  REQUIRE(doc.undo());  // the stroke, not the terrain's creation
  CHECK(height() == doctest::Approx(0.0f));
  CHECK(doc.terrain_grid(terrain).has_value());
  CHECK_FALSE(doc.dirty());
  std::filesystem::remove(path);
}

TEST_CASE("a terrain is sculpted, undone, saved and read back with its shape") {
  constexpr TerrainGrid kGrid{40.0f, 20};
  constexpr Color kGrass{0.3f, 0.5f, 0.2f, 1.0f};
  constexpr float kLift = 3.0f;
  constexpr TerrainBrush kHill{0.0f, 0.0f, 6.0f, kLift, SculptMode::Raise};
  const std::filesystem::path path = temp_copy_path("terrain");
  SceneDocument doc = SceneDocument::create_empty(path);
  const int terrain = doc.add_terrain("Terrain", kGrid, kGrass);
  REQUIRE(doc.terrain_grid(terrain).has_value());
  CHECK(doc.terrain_grid(terrain)->cells == kGrid.cells);
  CHECK(std::get<std::string>(doc.node(terrain).props.at("collider")) == "mesh");
  const int mesh = doc.node(terrain).mesh;
  const auto height = [&doc, mesh, &kGrid] {
    return terrain_height(doc.desc().meshes[static_cast<std::size_t>(mesh)].primitives.front(), kGrid, 0.0f, 0.0f);
  };
  const int cube = doc.add_primitive(PrimitiveShape::Cube, "Crate", kGrass);
  CHECK_FALSE(doc.terrain_grid(cube).has_value());
  CHECK_FALSE(doc.sculpt_terrain(cube, kHill));  // only terrains are sculpted
  std::string error;
  REQUIRE_MESSAGE(doc.save(error), error);
  const auto flat_size = std::filesystem::file_size(path);
  doc.take_changed_meshes();

  // One stroke, streamed in two steps, is one undo step.
  doc.checkpoint();
  REQUIRE(doc.sculpt_terrain(terrain, kHill));
  REQUIRE(doc.sculpt_terrain(terrain, kHill));
  CHECK(height() == doctest::Approx(kLift * 2.0f));
  CHECK(doc.dirty());
  CHECK(doc.take_changed_meshes() == std::vector<int>{mesh});  // the renderer is told once
  CHECK(doc.take_changed_meshes().empty());
  CHECK(doc.desc().meshes[static_cast<std::size_t>(mesh)].bounds.max.y == doctest::Approx(kLift * 2.0f));

  REQUIRE(doc.undo());
  CHECK(height() == doctest::Approx(0.0f));
  CHECK(doc.take_changed_meshes() == std::vector<int>{mesh});
  REQUIRE(doc.redo());
  CHECK(height() == doctest::Approx(kLift * 2.0f));

  // Saving writes the new shape over the old one: the file does not grow, and loads sculpted.
  REQUIRE_MESSAGE(doc.save(error), error);
  CHECK(std::filesystem::file_size(path) == flat_size);
  const LoadResult loaded = load_gltf_scene(path.string());
  REQUIRE(loaded.ok);
  const int node = loaded.scene.find_node("Terrain");
  REQUIRE(node != kNoIndex);
  const MeshDesc& saved =
      loaded.scene.meshes[static_cast<std::size_t>(loaded.scene.nodes[static_cast<std::size_t>(node)].mesh)];
  CHECK(terrain_height(saved.primitives.front(), kGrid, 0.0f, 0.0f) == doctest::Approx(kLift * 2.0f));
  CHECK(saved.bounds.max.y == doctest::Approx(kLift * 2.0f));

  // Opened again, it is still a terrain that can be sculpted.
  auto reopened = SceneDocument::open(path);
  REQUIRE(std::holds_alternative<SceneDocument>(reopened));
  SceneDocument& again = std::get<SceneDocument>(reopened);
  const int same = again.desc().find_node("Terrain");
  REQUIRE(again.terrain_grid(same).has_value());
  CHECK(again.sculpt_terrain(same, {0.0f, 0.0f, 6.0f, 1.0f, SculptMode::Lower}));
  std::filesystem::remove(path);
}

namespace {

// A copy of a .glb with its JSON changed by `change`, in a temporary file.
template <typename Change>
std::filesystem::path changed_copy(const std::filesystem::path& source, const std::string& name, Change change) {
  std::string error;
  std::optional<GlbFile> glb = read_glb(source, error);
  REQUIRE_MESSAGE(glb.has_value(), error);
  change(glb->json);
  const std::filesystem::path path = temp_copy_path(name);
  REQUIRE_MESSAGE(write_glb(path, *glb, error), error);
  return path;
}

// A .glb whose JSON chunk is `text`, written byte by byte (no JSON library walks it).
std::filesystem::path raw_glb(const std::string& name, std::string text) {
  constexpr std::uint32_t kMagic = 0x46546C67;
  constexpr std::uint32_t kVersion = 2;
  constexpr std::uint32_t kJsonChunk = 0x4E4F534A;
  text.resize(glb_padded_size(text.size()), ' ');
  const std::uint32_t header[] = {kMagic, kVersion,
                                  static_cast<std::uint32_t>(kGlbHeaderBytes + kGlbChunkHeaderBytes + text.size()),
                                  static_cast<std::uint32_t>(text.size()), kJsonChunk};
  const std::filesystem::path path = temp_copy_path(name);
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(header), sizeof(header));
  out << text;
  return path;
}

}  // namespace

TEST_CASE("a terrain whose file says its vertices start before the data is not saved over other memory") {
  constexpr TerrainGrid kGrid{40.0f, 20};
  constexpr Color kGrass{0.3f, 0.5f, 0.2f, 1.0f};
  constexpr TerrainBrush kHill{0.0f, 0.0f, 6.0f, 3.0f, SculptMode::Raise};
  constexpr int kBeforeTheData = -8;
  const std::filesystem::path made = temp_copy_path("terrain_source");
  SceneDocument fresh = SceneDocument::create_empty(made);
  fresh.add_terrain("Terrain", kGrid, kGrass);
  std::string error;
  REQUIRE_MESSAGE(fresh.save(error), error);
  // The loader reads a negative offset as zero, where the terrain's positions are: the scene opens.
  const std::filesystem::path path = changed_copy(made, "terrain_negative", [&](nlohmann::json& json) {
    const int accessor = json["meshes"][0]["primitives"][0]["attributes"]["POSITION"];
    const int view = json["accessors"][accessor]["bufferView"];
    REQUIRE(json["bufferViews"][view]["byteOffset"] == 0);
    json["bufferViews"][view]["byteOffset"] = kBeforeTheData;
  });
  auto opened = SceneDocument::open(path);
  REQUIRE(std::holds_alternative<SceneDocument>(opened));
  SceneDocument& doc = std::get<SceneDocument>(opened);
  const int terrain = doc.desc().find_node("Terrain");
  REQUIRE(doc.terrain_grid(terrain).has_value());
  REQUIRE(doc.sculpt_terrain(terrain, kHill));
  const auto size_before = std::filesystem::file_size(path);
  CHECK_FALSE(doc.save(error));
  CHECK(error.find("cannot save the shape") != std::string::npos);
  CHECK(std::filesystem::file_size(path) == size_before);  // the file is as it was
  std::filesystem::remove(made);
  std::filesystem::remove(path);
}

TEST_CASE("a scene whose 'scene' names none of its scenes opens and saves, or is refused: never fatal") {
  constexpr int kNoSuchScene = 40;
  // (The loader takes a negative "scene" as none given; the editor's reading of the file is its own.)
  const std::filesystem::path negative =
      changed_copy(fixture("hierarchy.glb"), "negative_scene", [](nlohmann::json& json) { json["scene"] = -1; });
  auto opened = SceneDocument::open(negative);
  if (auto* refused = std::get_if<std::string>(&opened)) FAIL(*refused);
  std::string error;
  CHECK_MESSAGE(std::get<SceneDocument>(opened).save(error), error);
  const LoadResult saved = load_gltf_scene(negative.string());
  REQUIRE_MESSAGE(saved.ok, saved.error);
  CHECK(saved.scene.find_node("Root") != kNoIndex);

  const std::filesystem::path beyond = changed_copy(fixture("hierarchy.glb"), "no_such_scene",
                                                    [&](nlohmann::json& json) { json["scene"] = kNoSuchScene; });
  CHECK(std::holds_alternative<std::string>(SceneDocument::open(beyond)));
  std::filesystem::remove(negative);
  std::filesystem::remove(beyond);
}

TEST_CASE("a model that holds something else where glTF wants an index is refused or imported, never fatal") {
  constexpr Color kGray{0.5f, 0.5f, 0.5f, 1.0f};
  const std::filesystem::path out = temp_copy_path("import_target");
  SceneDocument doc = SceneDocument::create_empty(out);
  doc.add_primitive(PrimitiveShape::Plane, "Floor", kGray);
  std::string error;
  REQUIRE_MESSAGE(doc.save(error), error);
  const std::size_t meshes_before = doc.desc().meshes.size();
  const int nodes_before = doc.node_count();

  // (The loader reads "null" as zero; the editor's own reading of the file is stricter.)
  const std::filesystem::path broken = changed_copy(fixture("hierarchy.glb"), "broken_model", [](nlohmann::json& json) {
    json["meshes"][0]["primitives"][0]["attributes"]["POSITION"] = nullptr;
  });
  CHECK(doc.import_model(broken, error) == kNoIndex);
  CHECK(error.find("cannot be used") != std::string::npos);
  // Nothing of the model stayed behind: the scene saves and loads as it was.
  CHECK(doc.desc().meshes.size() == meshes_before);
  CHECK(doc.node_count() == nodes_before);
  REQUIRE_MESSAGE(doc.save(error), error);
  const LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  CHECK(loaded.scene.meshes.size() == meshes_before);
  CHECK(loaded.scene.nodes.size() == static_cast<std::size_t>(nodes_before));

  const std::filesystem::path odd = changed_copy(fixture("hierarchy.glb"), "odd_model", [](nlohmann::json& json) {
    json["nodes"][0]["camera"] = nullptr;
    json["materials"][0]["normalTexture"] = {{"index", nullptr}};
  });
  CHECK_NOTHROW(doc.import_model(odd, error));
  CHECK_MESSAGE(doc.save(error), error);
  CHECK(load_gltf_scene(out.string()).ok);
  for (const auto& file : {out, broken, odd}) std::filesystem::remove(file);
}

TEST_CASE("a .glb whose JSON is nested very deep is refused when read") {
  constexpr std::size_t kVeryDeep = 100000;
  constexpr std::size_t kFewLevels = 12;
  const auto nested = [](std::size_t depth) {
    return R"({"asset":{"version":"2.0"},"extras":)" + std::string(depth, '[') + std::string(depth, ']') + "}";
  };
  std::string error;
  const std::filesystem::path deep = raw_glb("deep_json", nested(kVeryDeep));
  CHECK_FALSE(read_glb(deep, error).has_value());
  CHECK(error.find("nested") != std::string::npos);
  CHECK(std::holds_alternative<std::string>(SceneDocument::open(deep)));
  const std::filesystem::path shallow = raw_glb("shallow_json", nested(kFewLevels));
  CHECK(read_glb(shallow, error).has_value());
  std::filesystem::remove(deep);
  std::filesystem::remove(shallow);
}
