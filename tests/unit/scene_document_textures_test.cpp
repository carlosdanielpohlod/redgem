// Pictures put on a material's texture slots in the editor: carried into the scene's file, saved,
// read back by the engine, undone, and left out once nothing shows them.

#include <doctest/doctest.h>

#include <filesystem>
#include <variant>

#include "core/safe_path.hpp"
#include "editor/document/glb_file.hpp"
#include "editor/document/scene_document.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr std::uintmax_t kMaxFixtureBytes = 1 << 20;  // the pictures read here are far smaller

SceneDocument open_hierarchy() {
  auto opened = SceneDocument::open(fixture("hierarchy.glb"));
  if (auto* error = std::get_if<std::string>(&opened)) FAIL(*error);
  return std::move(std::get<SceneDocument>(opened));
}

std::vector<std::uint8_t> bytes_of(const std::string& picture) {
  const auto bytes = read_file_up_to(fixture(picture), kMaxFixtureBytes);
  REQUIRE(bytes);
  return *bytes;
}

int add(SceneDocument& doc, const std::string& picture) {
  std::string error;
  const int image = doc.add_image(fixture(picture), error);
  REQUIRE_MESSAGE(image != kNoIndex, error);
  return image;
}

// The material of the hierarchy's Child, with its slots changed by `change`.
template <typename Change>
void edit_material(SceneDocument& doc, Change change) {
  const int material = doc.material_of(doc.desc().find_node("Child"));
  MaterialDesc edited = doc.material(material);
  change(edited);
  doc.set_material(material, edited);
}

LoadResult saved_and_loaded(SceneDocument& doc, const std::string& name) {
  const std::filesystem::path out = std::filesystem::temp_directory_path() / ("redgem_textures_" + name + ".glb");
  std::string error;
  REQUIRE_MESSAGE(doc.save_as(out, error), error);
  LoadResult loaded = load_gltf_scene(out.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  std::filesystem::remove(out);
  return loaded;
}

const MaterialDesc& child_material(const SceneDesc& scene) {
  const NodeDesc& child = scene.nodes[scene.find_node("Child")];
  return scene.materials[scene.meshes[child.mesh].primitives.front().material];
}

}  // namespace

TEST_CASE("a picture file is added to the scene once, named after its file") {
  SceneDocument doc = open_hierarchy();
  const int images = static_cast<int>(doc.desc().images.size());
  const int image = add(doc, "images/sniff.png");
  CHECK(image == images);
  CHECK(doc.desc().images[image].encoded == bytes_of("images/sniff.png"));
  CHECK(doc.image_name(image) == "sniff.png");
  CHECK(add(doc, "images/sniff.png") == image);  // the same picture again
  CHECK(add(doc, "images/sniff.jpg") == image + 1);
  CHECK(doc.image_name(kNoIndex).empty());
  CHECK_FALSE(doc.dirty());  // nothing shows it yet
}

TEST_CASE("what is not a picture the engine shows is refused") {
  SceneDocument doc = open_hierarchy();
  std::string error;
  CHECK(doc.add_image(fixture("games/sandbox/scripts/probe.rb"), error) == kNoIndex);
  CHECK(error.find("probe.rb") != std::string::npos);
  CHECK(doc.add_image(fixture("images/missing.png"), error) == kNoIndex);
  CHECK(doc.add_image(fixture("images/animated.webp"), error) == kNoIndex);  // moving pictures are not
}

TEST_CASE("pictures set on a material's slots are saved, and the engine reads them back") {
  SceneDocument doc = open_hierarchy();
  const int color = add(doc, "images/sniff.png");
  const int bumps = add(doc, "images/sniff.jpg");
  const int glow = add(doc, "images/lossy.webp");
  edit_material(doc, [&](MaterialDesc& material) {
    material.base_color_image = color;
    material.normal_image = bumps;
    material.emissive_image = glow;
    material.occlusion_image = color;
    material.metallic_roughness_image = bumps;
  });
  CHECK(doc.dirty());
  const LoadResult loaded = saved_and_loaded(doc, "slots");
  const SceneDesc& saved = loaded.scene;
  const MaterialDesc& material = child_material(saved);
  REQUIRE(material.base_color_image != kNoIndex);
  CHECK(saved.images[material.base_color_image].encoded == bytes_of("images/sniff.png"));
  REQUIRE(material.normal_image != kNoIndex);
  CHECK(saved.images[material.normal_image].encoded == bytes_of("images/sniff.jpg"));
  REQUIRE(material.emissive_image != kNoIndex);
  CHECK(saved.images[material.emissive_image].encoded == bytes_of("images/lossy.webp"));
  CHECK(material.occlusion_image == material.base_color_image);  // one picture, one image
  CHECK(material.metallic_roughness_image == material.normal_image);
  CHECK(saved.images.size() == 3);
}

TEST_CASE("a slot set and undone, or cleared, leaves its picture out of the saved file") {
  SceneDocument doc = open_hierarchy();
  const int color = add(doc, "images/sniff.png");
  edit_material(doc, [&](MaterialDesc& material) { material.base_color_image = color; });
  REQUIRE(doc.undo());
  CHECK_FALSE(doc.dirty());
  CHECK(child_material(doc.desc()).base_color_image == kNoIndex);
  REQUIRE(doc.redo());
  CHECK(child_material(doc.desc()).base_color_image == color);
  CHECK(child_material(saved_and_loaded(doc, "redone").scene).base_color_image != kNoIndex);

  edit_material(doc, [](MaterialDesc& material) { material.base_color_image = kNoIndex; });
  const LoadResult cleared = saved_and_loaded(doc, "cleared");
  CHECK(child_material(cleared.scene).base_color_image == kNoIndex);
  CHECK(cleared.scene.images.empty());
}

TEST_CASE("a slot set to the picture it shows already is no edit") {
  SceneDocument doc = open_hierarchy();
  const int color = add(doc, "images/sniff.png");
  edit_material(doc, [&](MaterialDesc& material) { material.base_color_image = color; });
  REQUIRE(doc.can_undo());
  doc.checkpoint();
  const int material = doc.material_of(doc.desc().find_node("Child"));
  doc.set_material(material, doc.material(material), false);
  doc.drop_checkpoint_if_unchanged();
  REQUIRE(doc.undo());
  CHECK(child_material(doc.desc()).base_color_image == kNoIndex);  // the undo step left was the slot's
}

TEST_CASE("a textured object copied into another scene brings its picture along") {
  SceneDocument from = open_hierarchy();
  const int color = add(from, "images/sniff.png");
  edit_material(from, [&](MaterialDesc& material) { material.base_color_image = color; });
  std::string error;
  const auto clip = from.copy_nodes({from.desc().find_node("Root")}, error);
  REQUIRE_MESSAGE(clip, error);
  auto opened = SceneDocument::open(fixture("games/sandbox/scenes/level2.glb"));
  REQUIRE(std::holds_alternative<SceneDocument>(opened));
  SceneDocument into = std::move(std::get<SceneDocument>(opened));
  REQUIRE_MESSAGE(!into.paste_nodes(*clip, kNoIndex, error).empty(), error);
  const SceneDesc& saved = saved_and_loaded(into, "pasted_picture").scene;
  const MaterialDesc& material = child_material(saved);
  REQUIRE(material.base_color_image != kNoIndex);
  CHECK(saved.images[material.base_color_image].encoded == bytes_of("images/sniff.png"));
}
