#include "benchmark/field_scene.hpp"

#include <cmath>

#include "benchmark/field_textures.hpp"
#include "scene/shapes/primitives.hpp"

namespace redgem::benchmark {
namespace {

constexpr float kTuftWidth = 0.5f;  // meters
constexpr float kTuftHeight = 0.45f;
constexpr float kTrunkWidth = 0.35f;
constexpr float kTrunkHeight = 3.0f;
constexpr float kCrownWidth = 3.5f;
constexpr float kCrownHeight = 4.0f;
constexpr float kCrownBase = 2.2f;
constexpr int kCrossedQuads = 3;  // a tuft or a crown seen from anywhere
constexpr int kTrunkSides = 4;
constexpr float kGroundScale = 30.0f;  // of the 10 m plane
constexpr Vec3 kCameraAt{0.0f, 1.7f, 70.0f};
constexpr Vec3 kCameraLooksAt{0.0f, 0.0f, 0.0f};
constexpr float kFullTurn = 360.0f;

enum Material { kGround, kGrass, kBark, kLeaves };

// A standing quad, `width` x `height`, turned `degrees` around the vertical, its bottom at `base`.
void add_quad(PrimitiveDesc& mesh, float width, float height, float base, float degrees) {
  const float turn = degrees_to_radians(degrees);
  const Vec3 side = Vec3{std::cos(turn), 0.0f, std::sin(turn)} * (width * kHalf);
  const Vec3 normal{-std::sin(turn), 0.0f, std::cos(turn)};
  const Vec3 corners[] = {-side, side, side, -side};
  const float heights[] = {base, base, base + height, base + height};
  const float us[] = {0.0f, 1.0f, 1.0f, 0.0f};
  const float vs[] = {1.0f, 1.0f, 0.0f, 0.0f};
  const auto first = static_cast<std::uint32_t>(mesh.positions.size() / 3);
  for (int i = 0; i < 4; ++i) {
    mesh.positions.insert(mesh.positions.end(), {corners[i].x, heights[i], corners[i].z});
    mesh.normals.insert(mesh.normals.end(), {normal.x, normal.y, normal.z});
    mesh.texcoords.insert(mesh.texcoords.end(), {us[i], vs[i]});
  }
  mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
}

PrimitiveDesc crossed_quads(float width, float height, float base, int material) {
  PrimitiveDesc mesh;
  mesh.material = material;
  for (int i = 0; i < kCrossedQuads; ++i) add_quad(mesh, width, height, base, kFullTurn / 2 * i / kCrossedQuads);
  return mesh;
}

PrimitiveDesc trunk() {
  PrimitiveDesc mesh;
  mesh.material = kBark;
  for (int i = 0; i < kTrunkSides; ++i) add_quad(mesh, kTrunkWidth, kTrunkHeight, 0.0f, kFullTurn * i / kTrunkSides);
  return mesh;
}

MeshDesc mesh_of(const char* name, std::vector<PrimitiveDesc> primitives) {
  MeshDesc mesh{name, std::move(primitives), {}};
  for (const PrimitiveDesc& primitive : mesh.primitives) {
    for (std::size_t i = 0; i < primitive.positions.size(); i += 3)
      mesh.bounds.include(vec3_from(&primitive.positions[i]));
  }
  return mesh;
}

MaterialDesc material(Color color, int image) {
  MaterialDesc made;
  made.base_color = color;
  made.base_color_image = image;
  made.alpha_mode = image == kNoIndex ? AlphaMode::Opaque : AlphaMode::Mask;
  made.double_sided = image != kNoIndex;
  return made;
}

NodeDesc node(const char* name, int mesh, Vec3 position) {
  NodeDesc made;
  made.name = name;
  made.mesh = mesh;
  made.local.position = position;
  return made;
}

NodeDesc camera() {
  NodeDesc made = node("Camera", kNoIndex, kCameraAt);
  made.camera = CameraDesc{};
  made.local.rotation = quat_look_rotation(kCameraLooksAt - kCameraAt, Vec3::up());
  return made;
}

NodeDesc environment() {
  NodeDesc made = node("Environment", kNoIndex, Vec3::zero());
  made.props = {{"environment", true}, {"wind", 1.0}};
  return made;
}

Transform place_of(const Placement& placement) {
  Transform place;
  place.position = placement.position;
  place.rotation = quat_from_axis_angle(Vec3::up(), degrees_to_radians(placement.yaw_degrees));
  place.scale = Vec3::one() * placement.scale;
  return place;
}

NodeDesc placed(const char* name, int mesh, const Placement& placement) {
  NodeDesc made = node(name, mesh, placement.position);
  made.local = place_of(placement);
  return made;
}

ScatterSet scattered(int mesh, const std::vector<Placement>& placements) {
  ScatterSet set;
  set.mesh = mesh;
  for (const Placement& placement : placements) set.places.push_back(place_of(placement));
  return set;
}

}  // namespace

SceneDesc field_templates() {
  SceneDesc desc;
  desc.images = {grass_picture(), leaves_picture()};
  desc.materials = {material({0.25f, 0.45f, 0.15f, 1.0f}, kNoIndex), material({0.6f, 1.0f, 0.5f, 1.0f}, 0),
                    material({0.35f, 0.25f, 0.15f, 1.0f}, kNoIndex), material({0.5f, 0.9f, 0.4f, 1.0f}, 1)};
  PrimitiveDesc ground = make_primitive(PrimitiveShape::Plane);
  ground.material = kGround;
  desc.meshes = {mesh_of("Ground", {ground}),
                 mesh_of(kGrassName, {crossed_quads(kTuftWidth, kTuftHeight, 0.0f, kGrass)}),
                 mesh_of(kTreeName, {trunk(), crossed_quads(kCrownWidth, kCrownHeight, kCrownBase, kLeaves)})};
  NodeDesc ground_node = node("Ground", 0, Vec3::zero());
  ground_node.local.scale = Vec3::one() * kGroundScale;
  NodeDesc grass = node(kGrassName, kGrassMesh, Vec3::zero());
  NodeDesc tree = node(kTreeName, kTreeMesh, Vec3::zero());
  grass.props = tree.props = {{"prefab", true}};
  desc.nodes = {camera(), environment(), ground_node, grass, tree};
  return desc;
}

SceneDesc field_of_objects() {
  SceneDesc desc = field_templates();
  for (const Placement& placement : grass_placements()) desc.nodes.push_back(placed(kGrassName, kGrassMesh, placement));
  for (const Placement& placement : tree_placements()) desc.nodes.push_back(placed(kTreeName, kTreeMesh, placement));
  return desc;
}

void scatter_field(Scene& scene) {
  scene.scattered().add(scattered(kGrassMesh, grass_placements()));
  scene.scattered().add(scattered(kTreeMesh, tree_placements()));
}

}  // namespace redgem::benchmark
