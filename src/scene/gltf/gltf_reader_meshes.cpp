// Meshes of a glTF file, and the parts each is made of.

#include <algorithm>
#include <string>

#include "core/safe_path.hpp"
#include "scene/gltf/gltf_accessors.hpp"
#include "scene/gltf/gltf_reader.hpp"

namespace redgem {
namespace {

// Where the floats of an attribute are kept; null for the attributes that are not kept as floats.
std::vector<float>* floats_of(cgltf_attribute_type type, PrimitiveDesc& primitive) {
  if (type == cgltf_attribute_type_position) return &primitive.positions;
  if (type == cgltf_attribute_type_normal) return &primitive.normals;
  if (type == cgltf_attribute_type_texcoord) return &primitive.texcoords;
  return type == cgltf_attribute_type_weights ? &primitive.weights : nullptr;
}

// The kinds of element glTF gives the attributes that are read: what is read as three floats a
// vertex has to be three floats a vertex.
struct AttributeKind {
  cgltf_attribute_type attribute;
  cgltf_type kind;
};
constexpr AttributeKind kAttributeKinds[] = {
    {cgltf_attribute_type_position, cgltf_type_vec3}, {cgltf_attribute_type_normal, cgltf_type_vec3},
    {cgltf_attribute_type_texcoord, cgltf_type_vec2}, {cgltf_attribute_type_joints, cgltf_type_vec4},
    {cgltf_attribute_type_weights, cgltf_type_vec4},  {cgltf_attribute_type_color, cgltf_type_vec3},
    {cgltf_attribute_type_color, cgltf_type_vec4},
};

// Whether an attribute's elements are of a kind glTF gives it (any kind, for one that is not read).
bool right_kind(const cgltf_attribute& attribute) {
  bool read = false;
  for (const AttributeKind& known : kAttributeKinds) {
    if (known.attribute != attribute.type) continue;
    if (known.kind == attribute.data->type) return true;
    read = true;
  }
  return !read;
}

// The name a shape key the file does not name gets: the prefix and its number ("Key 2").
constexpr const char* kUnnamedTargetPrefix = "Key ";

// Where a shape key keeps the floats of an attribute; null for what is not read of it (tangents).
std::vector<float>* deltas_of(cgltf_attribute_type type, MorphTarget& target) {
  if (type == cgltf_attribute_type_position) return &target.positions;
  return type == cgltf_attribute_type_normal ? &target.normals : nullptr;
}

// Whether an attribute of a shape key is read (`deltas` is where it goes) as glTF gives it: three floats a vertex.
bool reads_as_deltas(const std::vector<float>* deltas, const cgltf_attribute& attribute) {
  return deltas != nullptr && attribute.data->type == cgltf_type_vec3;
}

MorphTarget read_target(const cgltf_morph_target& source) {
  MorphTarget target;
  for (cgltf_size a = 0; a < source.attributes_count; ++a) {
    const cgltf_attribute& attribute = source.attributes[a];
    std::vector<float>* deltas = deltas_of(attribute.type, target);
    if (reads_as_deltas(deltas, attribute)) *deltas = gltf_accessors::unpack_floats(attribute.data);
  }
  return target;
}

std::vector<MorphTarget> read_targets(const cgltf_primitive& primitive) {
  std::vector<MorphTarget> targets;
  for (cgltf_size t = 0; t < primitive.targets_count; ++t) targets.push_back(read_target(primitive.targets[t]));
  return targets;
}

// Whether a part has `count` shape keys, each with a delta for each of its vertices.
bool part_fits(const PrimitiveDesc& part, std::size_t count) {
  if (part.targets.size() != count) return false;
  return std::all_of(part.targets.begin(), part.targets.end(),
                     [&part](const MorphTarget& target) { return fits_primitive(target, part); });
}

bool targets_fit(const MeshDesc& mesh, std::size_t count) {
  return std::all_of(mesh.primitives.begin(), mesh.primitives.end(),
                     [count](const PrimitiveDesc& part) { return part_fits(part, count); });
}

bool names_target(const cgltf_mesh& source, std::size_t index) {
  return index < source.target_names_count && source.target_names[index] != nullptr;
}

std::string target_name(const cgltf_mesh& source, std::size_t index) {
  return names_target(source, index) ? printable(source.target_names[index])
                                     : kUnnamedTargetPrefix + std::to_string(index);
}

float target_weight(const cgltf_mesh& source, std::size_t index) {
  return index < source.weights_count ? source.weights[index] : 0.0f;
}

// Where a part's vertices reach with each of its shape keys applied whole.
void include_targets(Bounds& bounds, const PrimitiveDesc& primitive) {
  for (const MorphTarget& target : primitive.targets) {
    for (std::size_t f = 0; f + config::kFloatsPerPosition <= target.positions.size();
         f += config::kFloatsPerPosition) {
      bounds.include(vec3_from(&primitive.positions[f]) + vec3_from(&target.positions[f]));
    }
  }
}

// Only the first set of each attribute is kept (TEXCOORD_0, COLOR_0...).
bool is_first_set(const cgltf_attribute& attribute) { return attribute.index == 0; }

void read_attribute(const cgltf_attribute& attribute, PrimitiveDesc& out) {
  if (std::vector<float>* floats = floats_of(attribute.type, out)) {
    *floats = gltf_accessors::unpack_floats(attribute.data);
  }
  if (attribute.type == cgltf_attribute_type_color) out.colors = gltf_accessors::rgba_bytes(attribute.data);
  if (attribute.type == cgltf_attribute_type_joints) out.joints = gltf_accessors::joint_indices(attribute.data);
}

// False when one of the attributes that are read is not of the kind glTF gives it.
bool read_attributes(const cgltf_primitive& primitive, PrimitiveDesc& out) {
  for (cgltf_size a = 0; a < primitive.attributes_count; ++a) {
    const cgltf_attribute& attribute = primitive.attributes[a];
    if (!is_first_set(attribute)) continue;
    if (!right_kind(attribute)) return false;
    read_attribute(attribute, out);
  }
  return true;
}

std::size_t vertex_count_of(const PrimitiveDesc& primitive) {
  return primitive.positions.size() / config::kFloatsPerPosition;
}

void keep_primitive(MeshDesc& mesh, PrimitiveDesc& primitive) {
  for (std::size_t v = 0; v < vertex_count_of(primitive); ++v) {
    mesh.bounds.include(vec3_from(&primitive.positions[v * config::kFloatsPerPosition]));
  }
  include_targets(mesh.bounds, primitive);
  mesh.primitives.push_back(std::move(primitive));
}

}  // namespace

void GltfReader::load_meshes() {
  for (cgltf_size i = 0; i < data_.meshes_count; ++i) result_.scene.meshes.push_back(load_mesh(data_.meshes[i]));
}

MeshDesc GltfReader::load_mesh(const cgltf_mesh& mesh) {
  MeshDesc out;
  out.name = printable(text_or(mesh.name, ""));
  for (cgltf_size p = 0; p < mesh.primitives_count; ++p) {
    const cgltf_primitive& primitive = mesh.primitives[p];
    if (primitive.type != cgltf_primitive_type_triangles) {
      warn("mesh '" + out.name + "': skipping non-triangle primitive");
      continue;
    }
    if (std::optional<PrimitiveDesc> loaded = load_primitive(primitive, out.name)) keep_primitive(out, *loaded);
  }
  keep_targets(out, mesh);
  return out;
}

void GltfReader::keep_targets(MeshDesc& mesh, const cgltf_mesh& source) {
  const std::size_t count = mesh.primitives.empty() ? 0 : mesh.primitives.front().targets.size();
  if (!targets_fit(mesh, count)) {
    warn("mesh '" + mesh.name + "': its shape keys do not fit its parts alike, left out");
    for (PrimitiveDesc& part : mesh.primitives) part.targets.clear();
    return;
  }
  for (std::size_t t = 0; t < count; ++t) {
    mesh.target_names.push_back(target_name(source, t));
    mesh.target_weights.push_back(target_weight(source, t));
  }
}

std::nullopt_t GltfReader::dropped(const std::string& mesh_name, const std::string& why) {
  warn("mesh '" + mesh_name + "': a part is left out (" + why + ")");
  return std::nullopt;
}

std::optional<PrimitiveDesc> GltfReader::load_primitive(const cgltf_primitive& primitive,
                                                        const std::string& mesh_name) {
  PrimitiveDesc out;
  out.material = index_of(primitive.material, data_.materials);
  if (!read_attributes(primitive, out)) return dropped(mesh_name, "an attribute is not of the kind glTF gives it");
  if (out.positions.empty()) return dropped(mesh_name, "its positions cannot be read");
  ensure_normals(out, mesh_name);
  out.targets = read_targets(primitive);
  out.indices = gltf_accessors::triangle_corners(primitive.indices, vertex_count_of(out));
  if (out.indices.empty()) return dropped(mesh_name, "its triangles cannot be read");
  const std::string problem = primitive_shape_problem(out);
  if (!problem.empty()) return dropped(mesh_name, problem);
  return out;
}

void GltfReader::ensure_normals(PrimitiveDesc& primitive, const std::string& mesh_name) {
  if (primitive.normals.size() == primitive.positions.size()) return;
  warn("mesh '" + mesh_name + "': missing normals, using +Y");
  primitive.normals.assign(primitive.positions.size(), 0.0f);
  for (std::size_t v = 0; v < vertex_count_of(primitive); ++v) {
    primitive.normals[v * config::kFloatsPerNormal + kYOffset] = 1.0f;
  }
}

}  // namespace redgem
