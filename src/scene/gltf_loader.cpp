#include "scene/gltf_loader.hpp"

#include <array>
#include <algorithm>
#include <cgltf.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>

#include "scene/scene_config.hpp"
#include "core/safe_path.hpp"
#include "scene/extras_parser.hpp"
#include "scene/gltf_checks.hpp"

namespace redgem {
namespace {

constexpr int kRgbaComponents = 4;
// Where each channel sits in a color stored as floats.
constexpr int kRedChannel = 0;
constexpr int kGreenChannel = 1;
constexpr int kBlueChannel = 2;
constexpr int kAlphaChannel = 3;
constexpr const char* kJpegMimeType = "image/jpeg";
constexpr const char* kJpegExtension = ".jpg";
constexpr const char* kPngExtension = ".png";
constexpr float kOpaqueAlpha = 1.0f;
constexpr int kJointsPerVertex = config::kJointsPerVertex;
constexpr cgltf_uint kMaxJointByte = 255;  // the highest joint index a vertex can name
constexpr std::array<float, kFloatsPerMatrix> kIdentityMatrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

struct CgltfDeleter {
  void operator()(cgltf_data* data) const { cgltf_free(data); }
};
using CgltfPtr = std::unique_ptr<cgltf_data, CgltfDeleter>;

// Why cgltf could not read a file, in words.
struct ResultName {
  cgltf_result result = cgltf_result_success;
  const char* name = nullptr;
};
constexpr ResultName kResultNames[] = {
    {cgltf_result_success, "success"},
    {cgltf_result_data_too_short, "data too short"},
    {cgltf_result_unknown_format, "unknown format"},
    {cgltf_result_invalid_json, "invalid json"},
    {cgltf_result_invalid_gltf, "invalid glTF"},
    {cgltf_result_invalid_options, "invalid options"},
    {cgltf_result_file_not_found, "file not found"},
    {cgltf_result_io_error, "io error"},
    {cgltf_result_out_of_memory, "out of memory"},
    {cgltf_result_legacy_gltf, "legacy glTF"},
};

std::string describe(cgltf_result result) {
  for (const ResultName& known : kResultNames) {
    if (known.result == result) return known.name;
  }
  return "unknown error";
}

// The floats of an accessor; none for one that cannot be read (see gltf_checks::readable).
std::vector<float> unpack_floats(const cgltf_accessor* accessor) {
  if (!gltf_checks::readable(accessor)) return {};
  const cgltf_size components = cgltf_num_components(accessor->type);
  std::vector<float> out(accessor->count * components);
  cgltf_accessor_unpack_floats(accessor, out.data(), out.size());
  return out;
}

std::string text_or(const char* text, const std::string& fallback) { return text != nullptr ? text : fallback; }

Transform read_local_transform(const cgltf_node& node) {
  if (node.has_matrix) return transform_from_matrix(node.matrix);
  Transform t;
  if (node.has_translation) t.position = vec3_from(node.translation);
  if (node.has_rotation) t.rotation = normalize(quat_from(node.rotation));
  if (node.has_scale) t.scale = vec3_from(node.scale);
  return t;
}

std::optional<CameraDesc> read_camera(const cgltf_camera* camera) {
  if (camera == nullptr || camera->type != cgltf_camera_type_perspective) return std::nullopt;
  const auto& perspective = camera->data.perspective;
  return CameraDesc{perspective.yfov, perspective.znear, perspective.has_zfar ? perspective.zfar : 0.0f};
}

LightType light_type_of(cgltf_light_type type) {
  if (type == cgltf_light_type_point) return LightType::Point;
  return type == cgltf_light_type_spot ? LightType::Spot : LightType::Directional;
}

LightDesc read_light(const cgltf_light& light) {
  LightDesc out;
  out.type = light_type_of(light.type);
  out.color = {light.color[kRedChannel], light.color[kGreenChannel], light.color[kBlueChannel], kOpaqueAlpha};
  out.intensity = light.intensity;
  out.range = light.range;
  return out;
}

AlphaMode alpha_mode_of(cgltf_alpha_mode mode) {
  switch (mode) {
    case cgltf_alpha_mode_mask:
      return AlphaMode::Mask;
    case cgltf_alpha_mode_blend:
      return AlphaMode::Blend;
    default:
      return AlphaMode::Opaque;
  }
}

// Linear RGB, already multiplied by the emissive strength.
Vec3 emissive_of(const cgltf_material& material) {
  const float strength = material.has_emissive_strength ? material.emissive_strength.emissive_strength : 1.0f;
  return vec3_from(material.emissive_factor) * strength;
}

// What a channel animates; nullopt for morph target weights, which are not supported.
std::optional<AnimationPath> path_of(cgltf_animation_path_type type) {
  switch (type) {
    case cgltf_animation_path_type_translation:
      return AnimationPath::Translation;
    case cgltf_animation_path_type_rotation:
      return AnimationPath::Rotation;
    case cgltf_animation_path_type_scale:
      return AnimationPath::Scale;
    default:
      return std::nullopt;
  }
}

std::vector<std::uint8_t> embedded_bytes(const cgltf_buffer_view& view) {
  const auto* bytes = static_cast<const std::uint8_t*>(cgltf_buffer_view_data(&view));
  if (bytes == nullptr) return {};
  return {bytes, bytes + view.size};
}

std::vector<std::uint8_t> to_joint_bytes(const cgltf_accessor* accessor) {
  if (!gltf_checks::readable(accessor)) return {};
  std::vector<std::uint8_t> out(accessor->count * kJointsPerVertex);
  for (cgltf_size v = 0; v < accessor->count; ++v) {
    cgltf_uint joints[kJointsPerVertex] = {};
    cgltf_accessor_read_uint(accessor, v, joints, kJointsPerVertex);
    for (int j = 0; j < kJointsPerVertex; ++j) {
      out[v * kJointsPerVertex + j] = static_cast<std::uint8_t>(std::min<cgltf_uint>(joints[j], kMaxJointByte));
    }
  }
  return out;
}

std::vector<std::uint8_t> to_rgba_bytes(const cgltf_accessor* accessor) {
  const std::vector<float> floats = unpack_floats(accessor);
  if (floats.empty()) return {};
  const cgltf_size components = cgltf_num_components(accessor->type);
  std::vector<std::uint8_t> out(accessor->count * kRgbaComponents);
  for (cgltf_size v = 0; v < accessor->count; ++v) {
    for (int c = 0; c < kRgbaComponents; ++c) {
      const bool has_channel = c < static_cast<int>(components);
      const float value = has_channel ? floats[v * components + c] : kOpaqueAlpha;
      out[v * kRgbaComponents + c] = static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * config::kMaxColorByte);
    }
  }
  return out;
}

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

// Only the first set of each attribute is kept (TEXCOORD_0, COLOR_0...).
void read_attribute(const cgltf_attribute& attribute, PrimitiveDesc& out) {
  if (attribute.index != 0) return;
  if (std::vector<float>* floats = floats_of(attribute.type, out)) *floats = unpack_floats(attribute.data);
  if (attribute.type == cgltf_attribute_type_color) out.colors = to_rgba_bytes(attribute.data);
  if (attribute.type == cgltf_attribute_type_joints) out.joints = to_joint_bytes(attribute.data);
}

// The triangles' corners: as the file lists them (none when its list cannot be read), or every
// vertex in turn when it lists none.
std::vector<std::uint32_t> read_indices(const cgltf_primitive& primitive, std::size_t vertex_count) {
  const bool listed = primitive.indices != nullptr;
  if (listed && !gltf_checks::readable(primitive.indices)) return {};
  std::vector<std::uint32_t> indices(listed ? primitive.indices->count : vertex_count);
  for (std::size_t k = 0; k < indices.size(); ++k) {
    indices[k] = static_cast<std::uint32_t>(listed ? cgltf_accessor_read_index(primitive.indices, k) : k);
  }
  return indices;
}

// The accessor holding a skin's inverse bind matrices, when it has a 4x4 matrix of floats for
// each of `joints` joints; null otherwise (the joints then get the identity).
const cgltf_accessor* inverse_binds_of(const cgltf_skin& skin, cgltf_size joints) {
  const cgltf_accessor* matrices = skin.inverse_bind_matrices;
  const bool usable = gltf_checks::readable(matrices) && matrices->type == cgltf_type_mat4 &&
                      matrices->component_type == cgltf_component_type_r_32f && matrices->count >= joints;
  return usable ? matrices : nullptr;
}

std::array<float, kFloatsPerMatrix> inverse_bind_of(const cgltf_accessor* matrices, cgltf_size joint) {
  std::array<float, kFloatsPerMatrix> matrix = kIdentityMatrix;
  if (matrices != nullptr) cgltf_accessor_read_float(matrices, joint, matrix.data(), matrix.size());
  return matrix;
}

// A node waiting to be added to the scene: under which parent, and how deep that puts it.
struct PendingNode {
  const cgltf_node* node = nullptr;
  int parent = kNoIndex;
  int depth = 0;
};

class Loader {
 public:
  Loader(const cgltf_data& data, std::filesystem::path base_dir, LoadResult& result)
      : data_(data), base_dir_(std::move(base_dir)), result_(result) {}

  // How many joints of a skeleton are kept (see load_gltf_scene).
  void set_max_skin_joints(int joints) { max_skin_joints_ = static_cast<cgltf_uint>(std::max(joints, 1)); }

  // Fills the result; returns what made the file unusable, or an empty text.
  std::string run() {
    load_images();
    load_materials();
    load_meshes();
    limit_joints();
    load_nodes();
    load_skins();
    load_animations();
    return problem_;
  }

 private:
  template <typename T>
  int index_of(const T* item, const T* first) const {
    return item == nullptr ? kNoIndex : static_cast<int>(item - first);
  }

  void warn(std::string warning) { result_.warnings.push_back(std::move(warning)); }

  ImageData load_image(const cgltf_image& image) const {
    ImageData out;
    out.file_extension = text_or(image.mime_type, "") == kJpegMimeType ? kJpegExtension : kPngExtension;
    if (image.buffer_view != nullptr) {
      out.encoded = embedded_bytes(*image.buffer_view);
    } else if (const std::filesystem::path file = gltf_checks::file_of_uri(image.uri, base_dir_); !file.empty()) {
      // (Only a picture file of the scene's own folder, and not one of any size.)
      out.encoded = read_file_up_to(file, gltf_checks::kMaxImageFileBytes).value_or(std::vector<std::uint8_t>());
      if (file.has_extension()) out.file_extension = file.extension().string();
    }
    return out;
  }

  void load_images() {
    for (cgltf_size i = 0; i < data_.images_count; ++i) {
      ImageData image = load_image(data_.images[i]);
      if (image.encoded.empty()) warn("image " + std::to_string(i) + " has no readable data");
      result_.scene.images.push_back(std::move(image));
    }
  }

  int image_of(const cgltf_texture_view& view) const {
    if (view.texture == nullptr) return kNoIndex;
    // KHR_texture_basisu / webp images are not decodable here; fall back to the plain image.
    return index_of(view.texture->image, data_.images);
  }

  void read_pbr(const cgltf_material& material, MaterialDesc& out) const {
    if (!material.has_pbr_metallic_roughness) return;
    const auto& pbr = material.pbr_metallic_roughness;
    const float* color = pbr.base_color_factor;
    out.base_color = {color[kRedChannel], color[kGreenChannel], color[kBlueChannel], color[kAlphaChannel]};
    out.metallic = pbr.metallic_factor;
    out.roughness = pbr.roughness_factor;
    out.base_color_image = image_of(pbr.base_color_texture);
    out.metallic_roughness_image = image_of(pbr.metallic_roughness_texture);
  }

  // The pictures laid over the base color, and how strongly the first two count.
  void read_detail_textures(const cgltf_material& material, MaterialDesc& out) const {
    out.normal_image = image_of(material.normal_texture);
    if (out.normal_image != kNoIndex) out.normal_scale = material.normal_texture.scale;
    out.occlusion_image = image_of(material.occlusion_texture);
    if (out.occlusion_image != kNoIndex) out.occlusion_strength = material.occlusion_texture.scale;
    out.emissive_image = image_of(material.emissive_texture);
  }

  MaterialDesc load_material(const cgltf_material& material) const {
    MaterialDesc out;
    out.name = text_or(material.name, "");
    read_pbr(material, out);
    read_detail_textures(material, out);
    out.emissive = emissive_of(material);
    out.alpha_mode = alpha_mode_of(material.alpha_mode);
    out.alpha_cutoff = material.alpha_cutoff;
    out.double_sided = material.double_sided;
    return out;
  }

  void load_materials() {
    for (cgltf_size i = 0; i < data_.materials_count; ++i) {
      result_.scene.materials.push_back(load_material(data_.materials[i]));
    }
  }

  MeshDesc load_mesh(const cgltf_mesh& mesh) {
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
    return out;
  }

  void keep_primitive(MeshDesc& mesh, PrimitiveDesc& primitive) {
    const std::size_t vertex_count = primitive.positions.size() / config::kFloatsPerPosition;
    for (std::size_t v = 0; v < vertex_count; ++v) {
      mesh.bounds.include(vec3_from(&primitive.positions[v * config::kFloatsPerPosition]));
    }
    mesh.primitives.push_back(std::move(primitive));
  }

  void load_meshes() {
    for (cgltf_size i = 0; i < data_.meshes_count; ++i) result_.scene.meshes.push_back(load_mesh(data_.meshes[i]));
  }

  // Vertices of the joints that are not kept follow the last one that is.
  void limit_joints() {
    const auto last_kept = static_cast<std::uint8_t>(std::min(max_skin_joints_ - 1, kMaxJointByte));
    for (MeshDesc& mesh : result_.scene.meshes) {
      for (PrimitiveDesc& primitive : mesh.primitives) {
        for (std::uint8_t& joint : primitive.joints) joint = std::min(joint, last_kept);
      }
    }
  }

  // A mesh exported without normals is lit as if it faced up everywhere.
  void ensure_normals(PrimitiveDesc& primitive, const std::string& mesh_name) {
    if (primitive.normals.size() == primitive.positions.size()) return;
    warn("mesh '" + mesh_name + "': missing normals, using +Y");
    primitive.normals.assign(primitive.positions.size(), 0.0f);
    const std::size_t vertex_count = primitive.positions.size() / config::kFloatsPerPosition;
    for (std::size_t v = 0; v < vertex_count; ++v) primitive.normals[v * config::kFloatsPerNormal + kYOffset] = 1.0f;
  }

  // False when one of the attributes that are read is not of the kind glTF gives it.
  static bool read_attributes(const cgltf_primitive& primitive, PrimitiveDesc& out) {
    for (cgltf_size a = 0; a < primitive.attributes_count; ++a) {
      const cgltf_attribute& attribute = primitive.attributes[a];
      if (attribute.index == 0 && !right_kind(attribute)) return false;
      read_attribute(attribute, out);
    }
    return true;
  }

  std::nullopt_t dropped(const std::string& mesh_name, const std::string& why) {
    warn("mesh '" + mesh_name + "': a part is left out (" + why + ")");
    return std::nullopt;
  }

  // A part of a mesh, checked here once so that everything using the scene can trust it: its
  // vertices are whole, its triangles are whole and name only vertices it has. Anything else is
  // left out with a warning.
  std::optional<PrimitiveDesc> load_primitive(const cgltf_primitive& primitive, const std::string& mesh_name) {
    PrimitiveDesc out;
    out.material = index_of(primitive.material, data_.materials);
    if (!read_attributes(primitive, out)) return dropped(mesh_name, "an attribute is not of the kind glTF gives it");
    if (out.positions.empty()) return dropped(mesh_name, "its positions cannot be read");
    ensure_normals(out, mesh_name);
    out.indices = read_indices(primitive, out.positions.size() / config::kFloatsPerPosition);
    if (out.indices.empty()) return dropped(mesh_name, "its triangles cannot be read");
    const std::string problem = primitive_shape_problem(out);
    if (!problem.empty()) return dropped(mesh_name, problem);
    return out;
  }

  // glTF node index -> index in the scene description (nodes are stored parents-first).
  std::vector<int> node_indices() const {
    std::vector<int> indices(data_.nodes_count, kNoIndex);
    for (std::size_t i = 0; i < result_.scene.nodes.size(); ++i) {
      const int source = result_.scene.nodes[i].source_node;
      if (source != kNoIndex) indices[static_cast<std::size_t>(source)] = static_cast<int>(i);
    }
    return indices;
  }

  SkinDesc load_skin(const cgltf_skin& skin, const std::vector<int>& nodes) const {
    SkinDesc out;
    const cgltf_size joints = std::min<cgltf_size>(skin.joints_count, gltf_checks::kMaxJointsPerSkin);
    const cgltf_accessor* matrices = inverse_binds_of(skin, joints);
    for (cgltf_size j = 0; j < joints; ++j) {
      out.joints.push_back(nodes[static_cast<std::size_t>(index_of(skin.joints[j], data_.nodes))]);
      out.inverse_bind.push_back(inverse_bind_of(matrices, j));
    }
    return out;
  }

  void warn_of_extra_joints(const cgltf_skin& skin, cgltf_size index) {
    if (skin.inverse_bind_matrices != nullptr && inverse_binds_of(skin, skin.joints_count) == nullptr) {
      warn("skin " + std::to_string(index) + " has no usable inverse bind matrix for each of its joints: none is used");
    }
    if (skin.joints_count <= max_skin_joints_) return;
    warn("skin " + std::to_string(index) + " has more than " + std::to_string(max_skin_joints_) +
         " joints: vertices of the extra ones follow the last supported joint");
  }

  void load_skins() {
    const std::vector<int> nodes = node_indices();
    for (cgltf_size i = 0; i < data_.skins_count; ++i) {
      warn_of_extra_joints(data_.skins[i], i);
      result_.scene.skins.push_back(load_skin(data_.skins[i], nodes));
    }
    for (NodeDesc& node : result_.scene.nodes) {
      if (node.source_node == kNoIndex) continue;
      node.skin = index_of(data_.nodes[node.source_node].skin, data_.skins);
    }
  }

  // What a channel animates; nullopt (warned about when it is worth it) for one that cannot be played.
  std::optional<AnimationPath> playable_path(const cgltf_animation_channel& channel, const std::string& clip_name) {
    if (channel.target_node == nullptr || channel.sampler == nullptr) return std::nullopt;
    const std::optional<AnimationPath> path = path_of(channel.target_path);
    if (!path || channel.sampler->interpolation != cgltf_interpolation_type_cubic_spline) return path;
    warn("animation '" + clip_name + "': cubic spline keys are not supported, skipped");
    return std::nullopt;
  }

  std::optional<AnimationChannel> load_channel(const cgltf_animation_channel& channel, const std::string& clip_name,
                                               const std::vector<int>& nodes) {
    const std::optional<AnimationPath> path = playable_path(channel, clip_name);
    if (!path) return std::nullopt;
    AnimationChannel track;
    track.path = *path;
    track.node = nodes[static_cast<std::size_t>(index_of(channel.target_node, data_.nodes))];
    if (track.node == kNoIndex) return std::nullopt;
    track.times = unpack_floats(channel.sampler->input);
    track.values = unpack_floats(channel.sampler->output);
    if (track.times.empty()) return std::nullopt;
    return track;
  }

  AnimationDesc load_animation(const cgltf_animation& animation, cgltf_size index, const std::vector<int>& nodes) {
    AnimationDesc out;
    out.name = printable(text_or(animation.name, "animation_" + std::to_string(index)));
    for (cgltf_size c = 0; c < animation.channels_count; ++c) {
      std::optional<AnimationChannel> track = load_channel(animation.channels[c], out.name, nodes);
      if (!track) continue;
      out.duration = std::max(out.duration, track->times.back());
      out.channels.push_back(std::move(*track));
    }
    return out;
  }

  void load_animations() {
    const std::vector<int> nodes = node_indices();
    for (cgltf_size i = 0; i < data_.animations_count; ++i) {
      AnimationDesc clip = load_animation(data_.animations[i], i, nodes);
      if (!clip.channels.empty()) result_.scene.animations.push_back(std::move(clip));
    }
  }

  // The scene the file opens with, or its first one; null for a file that has none.
  const cgltf_scene* default_scene() const {
    if (data_.scene != nullptr) return data_.scene;
    return data_.scenes_count > 0 ? &data_.scenes[0] : nullptr;
  }

  void load_nodes() {
    const cgltf_scene* scene = default_scene();
    added_.assign(data_.nodes_count, false);
    if (scene == nullptr) return add_parentless_nodes();
    for (cgltf_size i = 0; i < scene->nodes_count; ++i) add_tree(*scene->nodes[i]);
  }

  void add_parentless_nodes() {
    for (cgltf_size i = 0; i < data_.nodes_count; ++i) {
      if (data_.nodes[i].parent == nullptr) add_tree(data_.nodes[i]);
    }
  }

  void warn_of_skipped_property(const std::string& node_name, const std::string& key) {
    warn("node '" + node_name + "': property '" + key + "' is not a scalar, ignored");
  }

  // A node's extras (Blender custom properties), with a warning for each thing left out.
  PropertyMap read_props(const cgltf_node& node, const std::string& node_name) {
    if (node.extras.data == nullptr) return {};
    ExtrasParseResult parsed = parse_extras(node.extras.data);
    if (!parsed.ok) warn("node '" + node_name + "': bad extras (" + parsed.error + ")");
    for (const std::string& key : parsed.skipped_keys) warn_of_skipped_property(node_name, key);
    return std::move(parsed.properties);
  }

  NodeDesc read_node(const cgltf_node& node, int parent, int index) {
    NodeDesc out;
    out.name = printable(text_or(node.name, "node_" + std::to_string(index)));
    out.parent = parent;
    out.local = read_local_transform(node);
    out.mesh = index_of(node.mesh, data_.meshes);
    out.camera = read_camera(node.camera);
    if (node.light != nullptr) out.light = read_light(*node.light);
    out.source_node = index_of(&node, data_.nodes);
    out.source_camera = index_of(node.camera, data_.cameras);
    out.source_light = index_of(node.light, data_.lights);
    out.props = read_props(node, out.name);
    return out;
  }

  // Adds a top-level node and everything under it, parents first and children in the file's
  // order. A list of work to do instead of a function calling itself: a file decides how deep its
  // nodes are nested, and must not decide how much stack is used.
  void add_tree(const cgltf_node& root) {
    std::vector<PendingNode> pending = {{&root, kNoIndex, 1}};
    while (!pending.empty() && problem_.empty()) {
      const PendingNode next = pending.back();
      pending.pop_back();
      add_node(next, pending);
    }
  }

  // Adds one node and queues its children. A node is added once, however often the file lists it.
  void add_node(const PendingNode& next, std::vector<PendingNode>& pending) {
    const auto source = static_cast<std::size_t>(index_of(next.node, data_.nodes));
    if (added_[source]) return;
    if (next.depth > gltf_checks::kMaxNodeDepth) {
      problem_ = "its nodes are nested more than " + std::to_string(gltf_checks::kMaxNodeDepth) + " deep";
      return;
    }
    added_[source] = true;
    const int index = static_cast<int>(result_.scene.nodes.size());
    result_.scene.nodes.push_back(read_node(*next.node, next.parent, index));
    if (next.parent != kNoIndex) result_.scene.nodes[next.parent].children.push_back(index);
    // (Last child first: the list hands back the first child next.)
    for (cgltf_size c = next.node->children_count; c > 0; --c) {
      pending.push_back({next.node->children[c - 1], index, next.depth + 1});
    }
  }

  const cgltf_data& data_;
  std::filesystem::path base_dir_;
  LoadResult& result_;
  std::vector<bool> added_;  // by glTF node index: already in the scene
  std::string problem_;      // what made the file unusable, when something did
  cgltf_uint max_skin_joints_ = config::kMaxSkinJoints;
};

// One step of opening a file: false (and why, in `error`) when it failed.
bool succeeded(cgltf_result status, const char* failure, const std::string& path, std::string& error) {
  if (status == cgltf_result_success) return true;
  error = std::string(failure) + " '" + path + "': " + describe(status);
  return false;
}

// One of the loader's own checks: false (and what is wrong, in `error`) when it found something.
bool sound(const std::string& problem, const std::string& path, std::string& error) {
  if (problem.empty()) return true;
  error = "invalid glTF '" + path + "': " + problem;
  return false;
}

std::filesystem::path folder_of(const std::string& path) { return std::filesystem::path(path).parent_path(); }

// The file parsed, with its buffers, and checked; null (and why, in `error`) when any of that
// fails. The loader's own checks come before the library's: those do sums that a file can make
// wrap around, and then read where the wrapped sum points.
CgltfPtr open_gltf(const std::string& path, std::string& error) {
  cgltf_options options{};
  cgltf_data* raw = nullptr;
  const cgltf_result parsed = cgltf_parse_file(&options, path.c_str(), &raw);
  CgltfPtr data(raw);
  const bool opened =
      succeeded(parsed, "cannot parse", path, error) &&
      sound(gltf_checks::buffer_file_problem(*data, folder_of(path)), path, error) &&
      succeeded(cgltf_load_buffers(&options, data.get(), path.c_str()), "cannot load buffers of", path, error) &&
      sound(gltf_checks::data_problem(*data), path, error) &&
      succeeded(cgltf_validate(data.get()), "invalid glTF", path, error);
  return opened ? std::move(data) : nullptr;
}

}  // namespace

LoadResult load_gltf_scene(const std::string& path, int max_skin_joints) {
  LoadResult result;
  const CgltfPtr data = open_gltf(path, result.error);
  if (data == nullptr) return result;
  Loader loader(*data, folder_of(path), result);
  loader.set_max_skin_joints(std::min(max_skin_joints, config::kMaxSkinJoints));
  result.ok = sound(loader.run(), path, result.error);
  if (!result.ok) result.scene = SceneDesc{};
  return result;
}

}  // namespace redgem
