// Several objects edited at once (one undo step each), and objects copied out of a scene and
// pasted into it or into another one.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <variant>

#include "core/safe_path.hpp"
#include "editor/document/glb_images.hpp"
#include "editor/document/gltf_json.hpp"
#include "editor/document/scene_document.hpp"
#include "editor/node_tree.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/props.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr float kTolerance = 1e-4f;
constexpr Vec3 kHolderPlace{10, 0, -4};               // where the object things are pasted under stands
constexpr std::uintmax_t kMaxFixtureBytes = 1 << 20;  // the pictures read here are far smaller

std::optional<std::vector<std::uint8_t>> read_file_bytes(const std::filesystem::path& file) {
  return read_file_up_to(file, kMaxFixtureBytes);
}

SceneDocument open_scene(const std::string& file) {
  auto opened = SceneDocument::open(fixture(file));
  if (auto* error = std::get_if<std::string>(&opened)) FAIL(*error);
  return std::move(std::get<SceneDocument>(opened));
}

std::filesystem::path temp_path(const std::string& name) {
  return std::filesystem::temp_directory_path() / ("redgem_clipboard_" + name + ".glb");
}

std::vector<std::uint8_t> copied(const SceneDocument& doc, const std::vector<std::string>& names) {
  std::vector<int> roots;
  for (const std::string& name : names) roots.push_back(doc.desc().find_node(name));
  std::string error;
  const auto bytes = doc.copy_nodes(roots, error);
  REQUIRE_MESSAGE(bytes, error);
  return *bytes;
}

std::vector<int> pasted(SceneDocument& doc, const std::vector<std::uint8_t>& bytes, int parent = kNoIndex) {
  std::string error;
  const std::vector<int> roots = doc.paste_nodes(bytes, parent, error);
  REQUIRE_MESSAGE(!roots.empty(), error);
  return roots;
}

LoadResult saved_and_loaded(SceneDocument& doc, const std::string& name) {
  const std::filesystem::path out = temp_path(name);
  std::string error;
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  std::filesystem::remove(out);
  return loaded;
}

Transform world_of(const SceneDocument& doc, const std::string& name) {
  return node_tree::world_of(doc.desc().nodes, doc.desc().find_node(name));
}

}  // namespace

TEST_CASE("several objects are duplicated and removed in one undo step each") {
  SceneDocument doc = open_scene("hierarchy.glb");
  const int count = doc.node_count();
  const std::vector<int> copies = doc.duplicate({doc.desc().find_node("Root"), doc.desc().find_node("Sun")});
  REQUIRE(copies.size() == 2);
  CHECK(doc.node(copies[0]).name == "Root.001");
  CHECK(doc.node(copies[1]).name == "Sun.001");
  CHECK(doc.node_count() == count + 3);  // Root with its Child, and Sun
  REQUIRE(doc.undo());
  CHECK(doc.node_count() == count);
  CHECK_FALSE(doc.can_undo());

  doc.remove({doc.desc().find_node("Root"), doc.desc().find_node("Camera")});
  CHECK(doc.desc().find_node("Child") == kNoIndex);
  CHECK(doc.desc().find_node("Camera") == kNoIndex);
  CHECK(doc.desc().find_node("Sun") != kNoIndex);
  REQUIRE(doc.undo());
  CHECK(doc.node_count() == count);
  CHECK_FALSE(doc.can_undo());
}

TEST_CASE("objects copied and pasted into the same scene come back whole, with new names") {
  SceneDocument doc = open_scene("hierarchy.glb");
  const int count = doc.node_count();
  const std::vector<std::uint8_t> clip = copied(doc, {"Root", "Sun"});
  const std::vector<int> roots = pasted(doc, clip);
  REQUIRE(roots.size() == 2);
  CHECK(doc.node_count() == count + 3);
  const NodeDesc& root = doc.node(roots[0]);
  CHECK(root.name == "Root.001");
  CHECK(root.parent == kNoIndex);
  CHECK(get_string(root.props, props::kScript) == std::optional<std::string>("spinner.rb"));
  CHECK(get_number(root.props, "speed") == std::optional<double>(2.5));
  REQUIRE(root.children.size() == 1);
  const NodeDesc& child = doc.node(root.children[0]);
  CHECK(child.name == "Child.001");
  CHECK(child.mesh != kNoIndex);
  CHECK(doc.node(roots[1]).light.has_value());  // a copied light stays a light
  // Where they were in the world.
  CHECK(approx_equal(world_of(doc, "Child.001").position, world_of(doc, "Child").position, kTolerance));

  // One undo step takes the whole paste away.
  REQUIRE(doc.undo());
  CHECK(doc.node_count() == count);
}

TEST_CASE("objects pasted into another scene keep their mesh, scripts and light after saving") {
  SceneDocument from = open_scene("hierarchy.glb");
  SceneDocument into = open_scene("games/sandbox/scenes/level2.glb");
  const std::vector<std::uint8_t> clip = copied(from, {"Root", "Sun", "Camera"});
  pasted(into, clip);
  const LoadResult loaded = saved_and_loaded(into, "between_scenes");
  const SceneDesc& saved = loaded.scene;
  const int root = saved.find_node("Root");
  REQUIRE(root != kNoIndex);
  CHECK(get_string(saved.nodes[root].props, props::kScript) == std::optional<std::string>("spinner.rb"));
  const int child = saved.find_node("Child");
  REQUIRE(child != kNoIndex);
  CHECK(saved.nodes[child].parent == root);
  REQUIRE(saved.nodes[child].mesh != kNoIndex);
  CHECK_FALSE(saved.meshes[saved.nodes[child].mesh].primitives.empty());
  const int sun = saved.find_node("Sun");
  REQUIRE(sun != kNoIndex);
  CHECK(saved.nodes[sun].light.has_value());
  const int camera = saved.find_node("Camera.001");  // level2 has a Camera of its own
  REQUIRE(camera != kNoIndex);
  CHECK(saved.nodes[camera].camera.has_value());
  CHECK(saved.find_node("Level2Ground") != kNoIndex);  // what the scene had stays
}

TEST_CASE("objects pasted under another one stay where they were in the world") {
  SceneDocument doc = open_scene("hierarchy.glb");
  const int holder = doc.add_empty("Holder");
  Transform place;
  place.position = kHolderPlace;
  doc.set_local(holder, place);
  const Transform before = world_of(doc, "Child");
  const std::vector<int> roots = pasted(doc, copied(doc, {"Root"}), holder);
  REQUIRE(roots.size() == 1);
  const NodeDesc& root = doc.node(roots[0]);
  CHECK(root.parent == holder);
  CHECK(std::find(doc.node(holder).children.begin(), doc.node(holder).children.end(), roots[0]) !=
        doc.node(holder).children.end());
  CHECK(approx_equal(world_of(doc, "Child.001").position, before.position, kTolerance));
}

TEST_CASE("a child copied without its parent is pasted at the top level where it was") {
  SceneDocument doc = open_scene("hierarchy.glb");
  const Transform before = world_of(doc, "Child");
  const std::vector<int> roots = pasted(doc, copied(doc, {"Child"}));
  REQUIRE(roots.size() == 1);
  CHECK(doc.node(roots[0]).parent == kNoIndex);
  CHECK(approx_equal(world_of(doc, "Child.001").position, before.position, kTolerance));
}

TEST_CASE("a character copied with its bones is pasted with a skeleton of its own") {
  SceneDocument from = open_scene("skinned.glb");
  SceneDocument into = open_scene("hierarchy.glb");
  pasted(into, copied(from, {"Rig"}));
  const LoadResult loaded = saved_and_loaded(into, "skinned_paste");
  const SceneDesc& saved = loaded.scene;
  REQUIRE(saved.skins.size() == 1);
  CHECK(saved.nodes[saved.skins[0].joints[0]].name == "Lower");
  CHECK(saved.nodes[saved.find_node("Arm")].skin == 0);
}

TEST_CASE("bytes that are not a scene paste nothing") {
  SceneDocument doc = open_scene("hierarchy.glb");
  const int count = doc.node_count();
  std::string error;
  CHECK(doc.paste_nodes({1, 2, 3}, kNoIndex, error).empty());
  CHECK_FALSE(error.empty());
  CHECK(doc.node_count() == count);
  CHECK_FALSE(doc.can_undo());
  error.clear();
  CHECK(doc.paste_nodes(copied(doc, {"Root"}), doc.node_count(), error).empty());  // no such parent
  CHECK_FALSE(error.empty());
}

TEST_CASE("pictures that are files next to the scene are carried inside a copy") {
  const std::filesystem::path folder = std::filesystem::temp_directory_path() / "redgem_clipboard_pictures";
  std::filesystem::remove_all(folder);
  std::filesystem::create_directories(folder);
  std::filesystem::copy_file(fixture("images/sniff.png"), folder / "wood.png");
  GlbFile glb;
  glb.json[gltf_json::kImages] =
      nlohmann::json::array({{{gltf_json::kUri, "wood.png"}}, {{gltf_json::kUri, "gone.png"}}});
  glb_images::embed_linked_images(glb, folder);
  const nlohmann::json& carried = glb.json[gltf_json::kImages][0];
  CHECK_FALSE(carried.contains(gltf_json::kUri));
  CHECK(carried["mimeType"] == "image/png");
  const nlohmann::json& view = glb.json[gltf_json::kBufferViews][carried[gltf_json::kBufferView].get<int>()];
  const auto file = read_file_bytes(folder / "wood.png");
  REQUIRE(file);
  CHECK(view[gltf_json::kByteLength].get<std::size_t>() == file->size());
  const std::size_t offset = view[gltf_json::kByteOffset].get<std::size_t>();
  CHECK(std::equal(file->begin(), file->end(), glb.bin.begin() + static_cast<std::ptrdiff_t>(offset)));
  CHECK(glb.json[gltf_json::kImages][1][gltf_json::kUri] == "gone.png");  // a missing file stays as it was
  std::filesystem::remove_all(folder);
}

TEST_CASE("a picture is added to a file as an image of its own") {
  GlbFile glb;
  const std::vector<std::uint8_t> png = *read_file_bytes(fixture("images/sniff.png"));
  CHECK(glb_images::mime_type_of(png) == "image/png");
  CHECK(glb_images::mime_type_of(*read_file_bytes(fixture("images/sniff.jpg"))) == "image/jpeg");
  CHECK(glb_images::mime_type_of(*read_file_bytes(fixture("images/lossy.webp"))) == "image/webp");
  CHECK(glb_images::mime_type_of({1, 2, 3}).empty());
  CHECK(glb_images::append_image(glb, png, "image/png") == 0);
  CHECK(glb_images::append_image(glb, png, "image/png") == 1);
  CHECK(glb.json[gltf_json::kImages][1][gltf_json::kBufferView] == 1);
}

TEST_CASE("a .glb goes through bytes in memory and back") {
  SceneDocument doc = open_scene("hierarchy.glb");
  const std::vector<std::uint8_t> clip = copied(doc, {"Root"});
  std::string error;
  const std::optional<GlbFile> decoded = decode_glb(clip, "the clipboard", error);
  REQUIRE_MESSAGE(decoded, error);
  const auto encoded = encode_glb(*decoded, error);
  REQUIRE_MESSAGE(encoded, error);
  CHECK(*encoded == clip);
  CHECK_FALSE(decode_glb({1, 2, 3}, "the clipboard", error));
  CHECK(error.find("the clipboard") != std::string::npos);
}
