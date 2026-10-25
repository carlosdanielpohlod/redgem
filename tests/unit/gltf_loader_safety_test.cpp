// What the glTF loader does with files that lie about themselves. Every file here is written by
// the test: a .gltf whose numbers claim more, or point elsewhere, than its data allows.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/image_header.hpp"
#include "scene/gltf/gltf_budget.hpp"
#include "scene/gltf/gltf_checks.hpp"
#include "scene/gltf/gltf_loader.hpp"

using namespace redgem;
using nlohmann::json;

namespace {

constexpr int kFloatComponent = 5126;
constexpr int kUnsignedShortComponent = 5123;
constexpr int kUnsignedByteComponent = 5121;
constexpr std::uint8_t kPastTheSkeleton = 5;  // a joint of a skeleton of two
constexpr std::size_t kTriangleCorners = 3;
constexpr std::size_t kFloatsPerPosition = 3;
constexpr float kNotIdentity = 2.0f;  // what the test's inverse bind matrix is filled with
// A count that makes "offset + stride * (count - 1) + size" wrap around to a small number.
constexpr std::uint64_t kWrappingCount = (std::uint64_t{1} << 62) + kTriangleCorners;
constexpr std::uint64_t kFarOffset = std::uint64_t{1} << 62;
constexpr std::size_t kRepeatedRoots = 1000;
// Room for a triangle's positions starting a few bytes in, at an offset no float may start at,
// and a distance between positions that is not a whole number of floats.
constexpr std::size_t kRoomyFloats = 10;
constexpr std::size_t kOddOffset = 2;
constexpr std::size_t kOddStride = 14;
constexpr const char* kPictureBytes = "not really a picture, but the bytes of a file";

// A folder of the test's own, removed when the test ends.
struct Scratch {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-gltf-safety-test";
  Scratch() {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "game");
  }
  ~Scratch() { std::filesystem::remove_all(dir); }
  Scratch(const Scratch&) = delete;
  Scratch& operator=(const Scratch&) = delete;

  std::filesystem::path game() const { return dir / "game"; }
};

// A .gltf as the test builds it: its JSON and the bytes of the data file beside it.
struct Gltf {
  json doc;
  std::vector<std::uint8_t> bin;

  // (Each view starts at a multiple of its values' size, as the glTF specification asks.)
  template <typename T>
  int add_view(const std::vector<T>& values) {
    bin.resize((bin.size() + sizeof(T) - 1) / sizeof(T) * sizeof(T));
    const std::size_t offset = bin.size();
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(values.data());
    bin.insert(bin.end(), bytes, bytes + values.size() * sizeof(T));
    doc["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", offset}, {"byteLength", values.size() * sizeof(T)}});
    return static_cast<int>(doc["bufferViews"].size()) - 1;
  }

  int add_accessor(int view, int component, std::size_t count, const char* type) {
    doc["accessors"].push_back({{"bufferView", view}, {"componentType", component}, {"count", count}, {"type", type}});
    return static_cast<int>(doc["accessors"].size()) - 1;
  }

  json& primitive() { return doc["meshes"][0]["primitives"][0]; }
};

// One triangle: accessor 0 its positions, accessor 1 its indices, on node 0 of the only scene.
Gltf triangle() {
  Gltf file;
  file.doc = {{"asset", {{"version", "2.0"}}},
              {"scene", 0},
              {"scenes", json::array({{{"nodes", {0}}}})},
              {"nodes", json::array({{{"name", "Triangle"}, {"mesh", 0}}})},
              {"bufferViews", json::array()},
              {"accessors", json::array()}};
  const int positions = file.add_accessor(file.add_view(std::vector<float>{0, 0, 0, 1, 0, 0, 0, 1, 0}), kFloatComponent,
                                          kTriangleCorners, "VEC3");
  const int indices = file.add_accessor(file.add_view(std::vector<std::uint16_t>{0, 1, 2}), kUnsignedShortComponent,
                                        kTriangleCorners, "SCALAR");
  file.doc["meshes"] =
      json::array({{{"name", "Mesh"},
                    {"primitives", json::array({{{"attributes", {{"POSITION", positions}}}, {"indices", indices}}})}}});
  return file;
}

// The first bytes of a PNG file saying its picture is `side` pixels square, and nothing more.
std::vector<std::uint8_t> png_header(std::uint32_t side) {
  std::vector<std::uint8_t> bytes = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n', 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
  for (int twice = 0; twice < 2; ++twice) {
    for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<std::uint8_t>(side >> shift));
  }
  bytes.insert(bytes.end(), {8, 6, 0, 0, 0});
  return bytes;
}

// Writes the file and its data into the scratch game folder; returns the .gltf's path.
std::string written(const Scratch& scratch, Gltf file) {
  file.doc["buffers"] = json::array({{{"uri", "scene.bin"}, {"byteLength", file.bin.size()}}});
  std::ofstream(scratch.game() / "scene.bin", std::ios::binary)
      .write(reinterpret_cast<const char*>(file.bin.data()), static_cast<std::streamsize>(file.bin.size()));
  std::ofstream(scratch.game() / "scene.gltf") << file.doc.dump();
  return (scratch.game() / "scene.gltf").string();
}

bool warned(const LoadResult& result, const std::string& text) {
  return std::any_of(result.warnings.begin(), result.warnings.end(),
                     [&text](const std::string& warning) { return warning.find(text) != std::string::npos; });
}

// A chain of `depth` nodes, each the only child of the one before; the first is the scene's root.
json chain_of_nodes(int depth) {
  json nodes = json::array();
  for (int i = 0; i < depth; ++i) {
    json node = {{"name", "n" + std::to_string(i)}};
    if (i + 1 < depth) node["children"] = {i + 1};
    nodes.push_back(node);
  }
  return nodes;
}

}  // namespace

TEST_CASE("a .gltf written by hand loads: one triangle on one object") {
  const Scratch scratch;
  const LoadResult result = load_gltf_scene(written(scratch, triangle()));
  REQUIRE_MESSAGE(result.ok, result.error);
  REQUIRE(result.scene.meshes.size() == 1);
  REQUIRE(result.scene.meshes[0].primitives.size() == 1);
  const PrimitiveDesc& primitive = result.scene.meshes[0].primitives[0];
  CHECK(primitive.positions.size() == kTriangleCorners * kFloatsPerPosition);
  CHECK(primitive.indices == std::vector<std::uint32_t>{0, 1, 2});
  CHECK(result.scene.find_node("Triangle") != kNoIndex);
}

TEST_CASE("an element count that makes the size check wrap around fails the load instead of overrunning memory") {
  const Scratch scratch;
  Gltf file = triangle();
  const int joints =
      file.add_accessor(file.add_view(std::vector<std::uint8_t>(kTriangleCorners * 4, 0)), 5121, 0, "VEC4");
  file.primitive()["attributes"]["JOINTS_0"] = joints;
  file.primitive().erase("indices");
  for (const int accessor : {0, joints}) file.doc["accessors"][accessor]["count"] = kWrappingCount;
  const LoadResult result = load_gltf_scene(written(scratch, file));
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("accessor") != std::string::npos);
  CHECK(result.scene.meshes.empty());
}

TEST_CASE("an accessor that claims more elements than the limit is refused, even with no data behind it") {
  const Scratch scratch;
  Gltf file = triangle();
  file.doc["accessors"][0].erase("bufferView");
  file.doc["accessors"][0]["count"] = gltf_checks::kMaxAccessorElements + 1;
  file.primitive().erase("indices");
  const LoadResult result = load_gltf_scene(written(scratch, file));
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("elements") != std::string::npos);
}

TEST_CASE("a scene whose parts are each within their limits but too big as a whole is refused before it is read") {
  const Scratch scratch;
  Gltf file = triangle();
  // Accessors as big as one may be, each a quarter of a gigabyte once read: enough of them are
  // more than a scene may need. (Their data is not there either: the size is what refuses them.)
  const std::uint64_t accessor_bytes = gltf_checks::kMaxAccessorElements * 4 * sizeof(float);
  const std::uint64_t accessors = gltf_budget::kMaxSceneBytes / accessor_bytes + 1;
  for (std::uint64_t i = 0; i < accessors; ++i)
    file.add_accessor(0, kFloatComponent, gltf_checks::kMaxAccessorElements, "VEC4");
  const LoadResult result = load_gltf_scene(written(scratch, file));
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("MB of memory, more than the 4096 MB a scene may") != std::string::npos);
  // Two fewer of them, and the scene is within the limit (and refused for what it really is).
  file.doc["accessors"].erase(file.doc["accessors"].size() - 1);
  file.doc["accessors"].erase(file.doc["accessors"].size() - 1);
  const LoadResult smaller = load_gltf_scene(written(scratch, file));
  CHECK_FALSE(smaller.ok);
  CHECK(smaller.error.find("reaches past its buffer view") != std::string::npos);
}

TEST_CASE("pictures of a few bytes that say they are huge count for what decoding them takes") {
  const Scratch scratch;
  Gltf file = triangle();
  // Each says it is as big as a picture may be: a quarter of a gigabyte once decoded.
  const std::uint64_t decoded = std::uint64_t{kMaxImageSide} * kMaxImageSide * 4;
  const std::uint64_t pictures = gltf_budget::kMaxSceneBytes / decoded + 1;
  for (std::uint64_t i = 0; i < pictures; ++i) {
    file.doc["images"].push_back({{"bufferView", file.add_view(png_header(kMaxImageSide))}, {"mimeType", "image/png"}});
  }
  const LoadResult result = load_gltf_scene(written(scratch, file));
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("with its pictures decoded it would take about") != std::string::npos);
  CHECK(result.scene.images.empty());
  // Two fewer, and the scene loads (nothing is decoded while it does).
  file.doc["images"].erase(file.doc["images"].size() - 1);
  file.doc["images"].erase(file.doc["images"].size() - 1);
  const LoadResult fewer = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(fewer.ok, fewer.error);
  CHECK(gltf_budget::decoded_bytes(fewer.scene.images) == decoded * (pictures - 2));
}

TEST_CASE("a file bigger than a scene may be is refused before it is read") {
  CHECK(gltf_budget::file_size_problem(gltf_budget::kMaxSceneBytes).empty());
  CHECK(gltf_budget::file_size_problem(gltf_budget::kMaxSceneBytes + 1) ==
        "the file alone takes 4096 MB of memory, more than the 4096 MB a scene may");
  // A file's own size is what is looked at (a scene's file of a few bytes is far from it).
  const Scratch scratch;
  CHECK(gltf_budget::file_problem(written(scratch, triangle())).empty());
  CHECK(gltf_budget::file_problem(scratch.game() / "missing.glb").empty());  // (the loader says it is missing)
}

TEST_CASE("an accessor without a buffer view is not read: the part of the mesh that needs it is left out") {
  const Scratch scratch;
  Gltf file = triangle();
  file.doc["accessors"][0].erase("bufferView");
  const LoadResult result = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(result.ok, result.error);
  REQUIRE(result.scene.meshes.size() == 1);
  CHECK(result.scene.meshes[0].primitives.empty());
  CHECK(warned(result, "positions cannot be read"));
}

TEST_CASE("data that ends after its buffer view fails the load") {
  const Scratch scratch;
  Gltf after = triangle();
  after.doc["accessors"][1]["count"] = kTriangleCorners * 2;  // six indices where the view holds three
  CHECK_FALSE(load_gltf_scene(written(scratch, after)).ok);

  Gltf far = triangle();
  far.doc["accessors"][0]["byteOffset"] = kFarOffset;
  CHECK_FALSE(load_gltf_scene(written(scratch, far)).ok);
}

TEST_CASE("data whose components do not lie at multiples of their size fails the load") {
  // (Found by the glTF fuzz target: floats read from odd addresses, which is undefined behaviour.)
  const Scratch scratch;
  Gltf offset = triangle();
  const int roomy = offset.add_view(std::vector<float>(kRoomyFloats, 0.0f));
  const int shifted = offset.add_accessor(roomy, kFloatComponent, kTriangleCorners, "VEC3");
  offset.doc["accessors"][shifted]["byteOffset"] = kOddOffset;
  offset.primitive()["attributes"]["POSITION"] = shifted;
  const LoadResult result = load_gltf_scene(written(scratch, offset));
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("not aligned") != std::string::npos);

  Gltf stride = triangle();
  stride.doc["bufferViews"][0]["byteStride"] = kOddStride;
  stride.doc["bufferViews"][0]["byteLength"] = kOddStride * kTriangleCorners;
  stride.bin.resize(kOddStride * kTriangleCorners + stride.bin.size());
  CHECK_FALSE(load_gltf_scene(written(scratch, stride)).ok);
}

TEST_CASE("a skin with fewer inverse bind matrices than joints uses none of them, instead of reading past them") {
  const Scratch scratch;
  Gltf file = triangle();
  const int matrices =
      file.add_accessor(file.add_view(std::vector<float>(kFloatsPerMatrix, kNotIdentity)), kFloatComponent, 1, "MAT4");
  file.doc["nodes"].push_back({{"name", "Lower"}});
  file.doc["nodes"].push_back({{"name", "Upper"}});
  file.doc["nodes"][0]["skin"] = 0;
  file.doc["scenes"][0]["nodes"] = {0, 1, 2};
  file.doc["skins"] = json::array({{{"joints", {1, 2}}, {"inverseBindMatrices", matrices}}});
  const LoadResult result = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(result.ok, result.error);
  REQUIRE(result.scene.skins.size() == 1);
  const SkinDesc& skin = result.scene.skins[0];
  REQUIRE(skin.inverse_bind.size() == 2);
  const std::array<float, kFloatsPerMatrix> identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  CHECK(skin.inverse_bind[0] == identity);
  CHECK(skin.inverse_bind[1] == identity);
  CHECK(warned(result, "inverse bind"));

  // With a matrix for each joint they are used.
  file.doc["skins"][0]["joints"] = {1};
  const LoadResult whole = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(whole.ok, whole.error);
  CHECK(whole.scene.skins[0].inverse_bind[0][0] == doctest::Approx(kNotIdentity));
}

TEST_CASE("a vertex naming a joint its skeleton does not have follows the skeleton's first, not another's bones") {
  const Scratch scratch;
  Gltf file = triangle();
  const std::vector<std::uint8_t> named = {1, 0, 0, 0, kPastTheSkeleton, 0, 0, 0, 0, 1, 0, 0};
  const int joints = file.add_accessor(file.add_view(named), kUnsignedByteComponent, kTriangleCorners, "VEC4");
  const int weights = file.add_accessor(file.add_view(std::vector<float>{1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}),
                                        kFloatComponent, kTriangleCorners, "VEC4");
  file.primitive()["attributes"]["JOINTS_0"] = joints;
  file.primitive()["attributes"]["WEIGHTS_0"] = weights;
  file.doc["nodes"].push_back({{"name", "Lower"}});
  file.doc["nodes"].push_back({{"name", "Upper"}});
  file.doc["nodes"][0]["skin"] = 0;
  file.doc["scenes"][0]["nodes"] = {0, 1, 2};
  file.doc["skins"] = json::array({{{"joints", {1, 2}}}});  // two joints: 0 and 1
  const LoadResult result = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(result.ok, result.error);
  const std::vector<std::uint16_t>& loaded = result.scene.meshes[0].primitives[0].joints;
  CHECK(loaded == std::vector<std::uint16_t>{1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0});
}

TEST_CASE("positions that are not three floats a vertex drop the part instead of miscounting its vertices") {
  const Scratch scratch;
  Gltf file = triangle();
  // Nine single floats: nine "elements" for the index check, three vertices for everything else.
  file.doc["accessors"][0]["type"] = "SCALAR";
  file.doc["accessors"][0]["count"] = kTriangleCorners * kFloatsPerPosition;
  file.doc["bufferViews"][1]["byteLength"] = kTriangleCorners * sizeof(std::uint16_t);
  const std::vector<std::uint16_t> far_indices = {0, 4, 8};
  std::memcpy(file.bin.data() + kTriangleCorners * kFloatsPerPosition * sizeof(float), far_indices.data(),
              far_indices.size() * sizeof(std::uint16_t));
  const LoadResult result = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.scene.meshes[0].primitives.empty());
  CHECK(warned(result, "left out"));
}

TEST_CASE("indices that do not make whole triangles drop the part") {
  const Scratch scratch;
  Gltf file = triangle();
  file.doc["accessors"][1]["count"] = kTriangleCorners - 1;
  const LoadResult result = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.scene.meshes[0].primitives.empty());
  CHECK(warned(result, "indices"));
}

TEST_CASE("every part the loader keeps has whole triangles of its own vertices") {
  const Scratch scratch;
  const LoadResult result = load_gltf_scene(written(scratch, triangle()));
  REQUIRE(result.ok);
  for (const MeshDesc& mesh : result.scene.meshes) {
    for (const PrimitiveDesc& primitive : mesh.primitives) CHECK(primitive_shape_problem(primitive).empty());
  }
}

TEST_CASE("objects nested as deep as the limit load, and one level deeper fails the load") {
  const Scratch scratch;
  Gltf file = triangle();
  file.doc["nodes"] = chain_of_nodes(gltf_checks::kMaxNodeDepth);
  const LoadResult at_limit = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(at_limit.ok, at_limit.error);
  CHECK(at_limit.scene.nodes.size() == static_cast<std::size_t>(gltf_checks::kMaxNodeDepth));
  CHECK(at_limit.scene.nodes.back().parent == gltf_checks::kMaxNodeDepth - 2);  // parents first, in order

  file.doc["nodes"] = chain_of_nodes(gltf_checks::kMaxNodeDepth + 1);
  const LoadResult too_deep = load_gltf_scene(written(scratch, file));
  CHECK_FALSE(too_deep.ok);
  CHECK(too_deep.error.find("nested") != std::string::npos);
  CHECK(too_deep.scene.nodes.empty());
}

TEST_CASE("a very long chain of objects fails the load without using up the stack") {
  constexpr int kVeryDeep = 90000;
  const Scratch scratch;
  Gltf file = triangle();
  file.doc["nodes"] = chain_of_nodes(kVeryDeep);
  CHECK_FALSE(load_gltf_scene(written(scratch, file)).ok);
}

TEST_CASE("a top-level object listed many times is one object, and children keep the file's order") {
  const Scratch scratch;
  Gltf file = triangle();
  file.doc["nodes"][0]["children"] = {1, 2};
  file.doc["nodes"].push_back({{"name", "First"}});
  file.doc["nodes"].push_back({{"name", "Second"}});
  file.doc["scenes"][0]["nodes"] = std::vector<int>(kRepeatedRoots, 0);
  const LoadResult result = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(result.ok, result.error);
  REQUIRE(result.scene.nodes.size() == 3);
  CHECK(result.scene.nodes[0].children == std::vector<int>{1, 2});
  CHECK(result.scene.nodes[1].name == "First");
  CHECK(result.scene.nodes[2].name == "Second");
}

TEST_CASE("a file with more objects than the limit fails the load") {
  const Scratch scratch;
  Gltf file = triangle();
  json nodes = json::array();
  for (std::size_t i = 0; i <= gltf_checks::kMaxSceneNodes; ++i) nodes.push_back(json::object());
  file.doc["nodes"] = nodes;
  const LoadResult result = load_gltf_scene(written(scratch, file));
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("nodes") != std::string::npos);
}

TEST_CASE("a picture is read from the scene's own folder and from nowhere else") {
  const Scratch scratch;
  std::filesystem::create_directories(scratch.game() / "textures");
  std::ofstream(scratch.game() / "textures" / "wood grain.png") << kPictureBytes;
  std::ofstream(scratch.dir / "secret.png") << kPictureBytes;  // beside the game's folder, not in it
  std::filesystem::create_symlink(scratch.dir / "secret.png", scratch.game() / "link.png");
  Gltf file = triangle();
  file.doc["images"] = json::array({{{"uri", "textures/wood%20grain.png"}},
                                    {{"uri", "../secret.png"}},
                                    {{"uri", "%2e%2e/secret.png"}},
                                    {{"uri", (scratch.dir / "secret.png").string()}},
                                    {{"uri", "link.png"}},
                                    {{"uri", "textures"}}});
  const LoadResult result = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(result.ok, result.error);
  REQUIRE(result.scene.images.size() == 6);
  CHECK(result.scene.images[0].encoded.size() == std::strlen(kPictureBytes));
  for (std::size_t outside = 1; outside < result.scene.images.size(); ++outside) {
    CAPTURE(outside);
    CHECK(result.scene.images[outside].encoded.empty());
  }
  CHECK(warned(result, "image 1 has no readable data"));
}

TEST_CASE("the files a scene names are found once for its load, and only in its own folder") {
  const Scratch scratch;
  std::ofstream(scratch.game() / "wood.png") << kPictureBytes;
  std::ofstream(scratch.dir / "secret.png") << kPictureBytes;
  Gltf file = triangle();
  file.doc["images"] = json::array(
      {{{"uri", "wood.png"}}, {{"uri", "../secret.png"}}, {{"uri", "data:image/png;base64,AAAA"}}, json::object()});
  const std::string path = written(scratch, file);
  cgltf_options options{};
  cgltf_data* data = nullptr;
  REQUIRE(cgltf_parse_file(&options, path.c_str(), &data) == cgltf_result_success);
  const gltf_checks::NamedFiles files = gltf_checks::named_files(*data, scratch.game());
  cgltf_free(data);
  const auto found = [&scratch](const char* name) { return std::filesystem::weakly_canonical(scratch.game() / name); };
  CHECK(files.buffers == std::vector<std::filesystem::path>{found("scene.bin")});
  CHECK(files.images == std::vector<std::filesystem::path>{found("wood.png"), {}, {}, {}});
}

TEST_CASE("a data file outside the scene's folder fails the load") {
  const Scratch scratch;
  Gltf file = triangle();
  const std::string path = written(scratch, file);
  std::filesystem::rename(scratch.game() / "scene.bin", scratch.dir / "outside.bin");
  json doc = file.doc;
  for (const char* uri : {"../outside.bin", "%2e%2e/outside.bin"}) {
    doc["buffers"] = json::array({{{"uri", uri}, {"byteLength", file.bin.size()}}});
    std::ofstream(path) << doc.dump();
    const LoadResult result = load_gltf_scene(path);
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("buffer 0") != std::string::npos);
  }
}

TEST_CASE("names from a file are shown without their control characters") {
  const Scratch scratch;
  Gltf file = triangle();
  file.doc["nodes"][0]["name"] = "Tri@[2Jangle@error: fake";
  file.doc["meshes"][0]["name"] = "Me@sh";
  const std::string path = written(scratch, file);
  // (The control characters go into the file as they are: a JSON library would write them escaped.)
  // (Byte for byte: Windows would write each line break as two characters otherwise.)
  std::stringstream as_written;
  as_written << std::ifstream(path, std::ios::binary).rdbuf();
  std::string text = as_written.str();
  const char controls[] = {'\x1b', '\n', '\r'};
  std::size_t next = 0;
  for (std::size_t at = text.find('@'); at != std::string::npos; at = text.find('@')) text[at] = controls[next++];
  std::ofstream(path, std::ios::binary) << text;
  const LoadResult result = load_gltf_scene(path);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.scene.nodes[0].name == "Tri?[2Jangle?error: fake");
  CHECK(result.scene.meshes[0].name == "Me?sh");
}

TEST_CASE("elements fit a buffer only when no part of the sum wraps around") {
  constexpr std::size_t kSize = 100;
  constexpr std::size_t kHuge = ~std::size_t{0} - 4;
  CHECK(gltf_checks::elements_fit(0, 12, 3, 12, 36));
  CHECK_FALSE(gltf_checks::elements_fit(0, 12, 4, 12, 36));
  CHECK(gltf_checks::elements_fit(kSize, 4, 0, 4, kSize));  // nothing to read
  CHECK_FALSE(gltf_checks::elements_fit(kHuge, 1, 2, 1, kSize));
  CHECK_FALSE(gltf_checks::elements_fit(0, kHuge, 3, 4, kSize));
  CHECK_FALSE(gltf_checks::elements_fit(0, 4, kHuge, 4, kSize));
  CHECK_FALSE(gltf_checks::elements_fit(8, 4, 1, kHuge, kSize));
}

TEST_CASE("a material's custom property says whether it is dithered; any other is mixed, as glTF's blend says") {
  const Scratch scratch;
  Gltf file = triangle();
  file.doc["materials"] =
      json::array({{{"name", "Hair"}, {"alphaMode", "BLEND"}, {"extras", {{"transparency", "dithered"}}}},
                   {{"name", "Glass"}, {"alphaMode", "BLEND"}, {"extras", {{"transparency", "glass"}}}},
                   {{"name", "Plain"}}});
  const LoadResult result = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(result.ok, result.error);
  REQUIRE(result.scene.materials.size() == 3);
  CHECK(result.scene.materials[0].dithered);
  CHECK(result.scene.materials[0].alpha_mode == AlphaMode::Blend);
  CHECK_FALSE(result.scene.materials[1].dithered);
  CHECK_FALSE(result.scene.materials[2].dithered);
}

TEST_CASE("clearcoat imports its factors and three independent texture slots") {
  constexpr float kCoat = 0.8f;
  constexpr float kRoughness = 0.2f;
  constexpr float kNormalScale = 0.6f;
  constexpr int kCoatMap = 0;
  constexpr int kRoughnessMap = 1;
  constexpr int kNormalMap = 2;
  const Scratch scratch;
  Gltf file = triangle();
  const int image = file.add_view(png_header(1));
  file.doc["images"] = json::array({{{"bufferView", image}, {"mimeType", "image/png"}},
                                    {{"bufferView", image}, {"mimeType", "image/png"}},
                                    {{"bufferView", image}, {"mimeType", "image/png"}}});
  file.doc["textures"] = json::array({{{"source", kCoatMap}}, {{"source", kRoughnessMap}}, {{"source", kNormalMap}}});
  const json extension = {{"clearcoatFactor", kCoat},
                          {"clearcoatRoughnessFactor", kRoughness},
                          {"clearcoatTexture", {{"index", kCoatMap}}},
                          {"clearcoatRoughnessTexture", {{"index", kRoughnessMap}}},
                          {"clearcoatNormalTexture", {{"index", kNormalMap}, {"scale", kNormalScale}}}};
  file.doc["materials"] = json::array({{{"extensions", {{"KHR_materials_clearcoat", extension}}}}, json::object()});
  const LoadResult result = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(result.ok, result.error);
  REQUIRE(result.scene.materials.size() == 2);
  const auto& coat = result.scene.materials.front().clearcoat;
  CHECK(coat.factor == doctest::Approx(kCoat));
  CHECK(coat.roughness == doctest::Approx(kRoughness));
  CHECK(coat.factor_image == kCoatMap);
  CHECK(coat.roughness_image == kRoughnessMap);
  CHECK(coat.normal_image == kNormalMap);
  CHECK(coat.normal_scale == doctest::Approx(kNormalScale));
  const auto& plain = result.scene.materials.back().clearcoat;
  CHECK(plain.factor == 0.0f);
  CHECK(plain.factor_image == kNoIndex);
  CHECK(plain.roughness_image == kNoIndex);
  CHECK(plain.normal_image == kNoIndex);
}

TEST_CASE("water material extras import bounded physical controls without changing plain materials") {
  const Scratch scratch;
  Gltf file = triangle();
  constexpr float kHeight = 0.15f;
  constexpr float kLength = 4.0f;
  constexpr float kRedAbsorption = 0.7f;
  file.doc["materials"] = json::array({{{"extras",
                                         {{"water", true},
                                          {"wave_height", kHeight},
                                          {"wavelength", kLength},
                                          {"absorption_red", kRedAbsorption},
                                          {"foam_width", 0.0}}}},
                                       json::object()});
  const LoadResult result = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(result.ok, result.error);
  const WaterDesc& water = result.scene.materials.front().water;
  CHECK(water.enabled);
  CHECK(water.wave_height == doctest::Approx(kHeight));
  CHECK(water.wavelength == doctest::Approx(kLength));
  CHECK(water.absorption.x == doctest::Approx(kRedAbsorption));
  CHECK(water.foam_width == 0.0f);
  CHECK_FALSE(result.scene.materials.back().water.enabled);
}

TEST_CASE("invalid water lengths and negative coefficients are kept in the supported range") {
  constexpr double kEnormousRefraction = 1e300;
  constexpr float kRefractionLimit = 0.1f;
  const Scratch scratch;
  Gltf file = triangle();
  file.doc["materials"] = json::array({{{"extras",
                                         {{"water", 1},
                                          {"wave_height", -1.0},
                                          {"wavelength", 0.0},
                                          {"wave_speed", -1.0},
                                          {"absorption_red", -1.0},
                                          {"refraction", kEnormousRefraction}}}}});
  const LoadResult result = load_gltf_scene(written(scratch, file));
  REQUIRE_MESSAGE(result.ok, result.error);
  const WaterDesc& water = result.scene.materials.front().water;
  CHECK(water.enabled);
  CHECK(water.wave_height == 0.0f);
  CHECK(water.wavelength > 0.0f);
  CHECK(water.speed == 0.0f);
  CHECK(water.absorption.x == 0.0f);
  CHECK(water.refraction <= kRefractionLimit);
}
