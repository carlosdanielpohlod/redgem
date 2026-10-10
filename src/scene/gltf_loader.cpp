#include "scene/gltf_loader.hpp"

#include <array>
#include <algorithm>
#include <cgltf.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <unordered_map>

#include "core/config.hpp"
#include "scene/extras_parser.hpp"

namespace redgem {
namespace {

constexpr int kRgbaComponents = 4;
constexpr int kAlphaChannel = 3;
constexpr float kOpaqueAlpha = 1.0f;
constexpr int kJointsPerVertex = config::kJointsPerVertex;
constexpr cgltf_uint kMaxJointIndex = config::kMaxSkinJoints - 1;
constexpr std::array<float, 16> kIdentityMatrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

struct CgltfDeleter {
  void operator()(cgltf_data* data) const { cgltf_free(data); }
};
using CgltfPtr = std::unique_ptr<cgltf_data, CgltfDeleter>;

std::string describe(cgltf_result result) {
  switch (result) {
    case cgltf_result_success: return "success";
    case cgltf_result_data_too_short: return "data too short";
    case cgltf_result_unknown_format: return "unknown format";
    case cgltf_result_invalid_json: return "invalid json";
    case cgltf_result_invalid_gltf: return "invalid glTF";
    case cgltf_result_invalid_options: return "invalid options";
    case cgltf_result_file_not_found: return "file not found";
    case cgltf_result_io_error: return "io error";
    case cgltf_result_out_of_memory: return "out of memory";
    case cgltf_result_legacy_gltf: return "legacy glTF";
    default: return "unknown error";
  }
}

std::vector<float> unpack_floats(const cgltf_accessor* accessor) {
  const cgltf_size components = cgltf_num_components(accessor->type);
  std::vector<float> out(accessor->count * components);
  cgltf_accessor_unpack_floats(accessor, out.data(), out.size());
  return out;
}

Transform read_local_transform(const cgltf_node& node) {
  if (node.has_matrix) return transform_from_matrix(node.matrix);
  Transform t;
  if (node.has_translation) t.position = {node.translation[0], node.translation[1], node.translation[2]};
  if (node.has_rotation) {
    t.rotation = normalize(Quat{node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]});
  }
  if (node.has_scale) t.scale = {node.scale[0], node.scale[1], node.scale[2]};
  return t;
}

class Loader {
 public:
  Loader(const cgltf_data& data, std::filesystem::path base_dir, LoadResult& result)
      : data_(data), base_dir_(std::move(base_dir)), result_(result) {}

  void run() {
    load_images();
    load_materials();
    load_meshes();
    load_nodes();
    load_skins();
    load_animations();
  }

 private:
  template <typename T>
  int index_of(const T* item, const T* first) const {
    return item == nullptr ? kNoIndex : static_cast<int>(item - first);
  }

  void load_images() {
    for (cgltf_size i = 0; i < data_.images_count; ++i) {
      const cgltf_image& image = data_.images[i];
      ImageData out;
      const std::string mime = image.mime_type ? image.mime_type : "";
      out.file_extension = mime == "image/jpeg" ? ".jpg" : ".png";
      if (image.buffer_view != nullptr) {
        const auto* bytes = static_cast<const std::uint8_t*>(cgltf_buffer_view_data(image.buffer_view));
        if (bytes != nullptr) out.encoded.assign(bytes, bytes + image.buffer_view->size);
      } else if (image.uri != nullptr && std::string(image.uri).rfind("data:", 0) != 0) {
        const std::filesystem::path file = base_dir_ / image.uri;
        std::ifstream in(file, std::ios::binary);
        out.encoded.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        if (file.has_extension()) out.file_extension = file.extension().string();
      }
      if (out.encoded.empty()) result_.warnings.push_back("image " + std::to_string(i) + " has no readable data");
      result_.scene.images.push_back(std::move(out));
    }
  }

  int image_of(const cgltf_texture_view& view) const {
    if (view.texture == nullptr) return kNoIndex;
    // KHR_texture_basisu / webp images are not decodable here; fall back to the plain image.
    return index_of(view.texture->image, data_.images);
  }

  void load_materials() {
    for (cgltf_size i = 0; i < data_.materials_count; ++i) {
      const cgltf_material& material = data_.materials[i];
      MaterialDesc out;
      out.name = material.name ? material.name : "";
      if (material.has_pbr_metallic_roughness) {
        const auto& pbr = material.pbr_metallic_roughness;
        out.base_color = {pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2],
                          pbr.base_color_factor[kAlphaChannel]};
        out.metallic = pbr.metallic_factor;
        out.roughness = pbr.roughness_factor;
        out.base_color_image = image_of(pbr.base_color_texture);
        out.metallic_roughness_image = image_of(pbr.metallic_roughness_texture);
      }
      out.normal_image = image_of(material.normal_texture);
      if (out.normal_image != kNoIndex) out.normal_scale = material.normal_texture.scale;
      out.occlusion_image = image_of(material.occlusion_texture);
      if (out.occlusion_image != kNoIndex) out.occlusion_strength = material.occlusion_texture.scale;
      out.emissive_image = image_of(material.emissive_texture);
      const float strength = material.has_emissive_strength ? material.emissive_strength.emissive_strength : 1.0f;
      out.emissive = Vec3{material.emissive_factor[0], material.emissive_factor[1], material.emissive_factor[2]} * strength;
      out.alpha_mode = material.alpha_mode == cgltf_alpha_mode_mask    ? AlphaMode::Mask
                       : material.alpha_mode == cgltf_alpha_mode_blend ? AlphaMode::Blend
                                                                       : AlphaMode::Opaque;
      out.alpha_cutoff = material.alpha_cutoff;
      out.double_sided = material.double_sided;
      result_.scene.materials.push_back(std::move(out));
    }
  }

  void load_meshes() {
    for (cgltf_size i = 0; i < data_.meshes_count; ++i) {
      const cgltf_mesh& mesh = data_.meshes[i];
      MeshDesc out;
      out.name = mesh.name ? mesh.name : "";
      for (cgltf_size p = 0; p < mesh.primitives_count; ++p) {
        const cgltf_primitive& primitive = mesh.primitives[p];
        if (primitive.type != cgltf_primitive_type_triangles) {
          result_.warnings.push_back("mesh '" + out.name + "': skipping non-triangle primitive");
          continue;
        }
        out.primitives.push_back(load_primitive(primitive, out));
      }
      result_.scene.meshes.push_back(std::move(out));
    }
  }

  PrimitiveDesc load_primitive(const cgltf_primitive& primitive, MeshDesc& mesh) {
    PrimitiveDesc out;
    out.material = index_of(primitive.material, data_.materials);
    for (cgltf_size a = 0; a < primitive.attributes_count; ++a) {
      const cgltf_attribute& attribute = primitive.attributes[a];
      if (attribute.index != 0) continue;
      switch (attribute.type) {
        case cgltf_attribute_type_position: out.positions = unpack_floats(attribute.data); break;
        case cgltf_attribute_type_normal: out.normals = unpack_floats(attribute.data); break;
        case cgltf_attribute_type_texcoord: out.texcoords = unpack_floats(attribute.data); break;
        case cgltf_attribute_type_color: out.colors = to_rgba_bytes(attribute.data); break;
        case cgltf_attribute_type_joints: out.joints = to_joint_bytes(attribute.data); break;
        case cgltf_attribute_type_weights: out.weights = unpack_floats(attribute.data); break;
        default: break;
      }
    }
    const std::size_t vertex_count = out.positions.size() / config::kFloatsPerPosition;
    if (out.normals.size() != out.positions.size()) {
      result_.warnings.push_back("mesh '" + mesh.name + "': missing normals, using +Y");
      out.normals.assign(out.positions.size(), 0.0f);
      for (std::size_t v = 0; v < vertex_count; ++v) out.normals[v * config::kFloatsPerNormal + 1] = 1.0f;
    }
    if (primitive.indices != nullptr) {
      out.indices.resize(primitive.indices->count);
      for (cgltf_size k = 0; k < primitive.indices->count; ++k) {
        out.indices[k] = static_cast<std::uint32_t>(cgltf_accessor_read_index(primitive.indices, k));
      }
    } else {
      out.indices.resize(vertex_count);
      for (std::size_t k = 0; k < vertex_count; ++k) out.indices[k] = static_cast<std::uint32_t>(k);
    }
    for (std::size_t v = 0; v < vertex_count; ++v) {
      const float* p = &out.positions[v * config::kFloatsPerPosition];
      mesh.bounds.include({p[0], p[1], p[2]});
    }
    return out;
  }

  static std::vector<std::uint8_t> to_joint_bytes(const cgltf_accessor* accessor) {
    std::vector<std::uint8_t> out(accessor->count * kJointsPerVertex);
    for (cgltf_size v = 0; v < accessor->count; ++v) {
      cgltf_uint joints[kJointsPerVertex] = {};
      cgltf_accessor_read_uint(accessor, v, joints, kJointsPerVertex);
      for (int j = 0; j < kJointsPerVertex; ++j) {
        out[v * kJointsPerVertex + j] = static_cast<std::uint8_t>(std::min<cgltf_uint>(joints[j], kMaxJointIndex));
      }
    }
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

  void load_skins() {
    const std::vector<int> nodes = node_indices();
    for (cgltf_size i = 0; i < data_.skins_count; ++i) {
      const cgltf_skin& skin = data_.skins[i];
      SkinDesc out;
      if (skin.joints_count > kMaxJointIndex + 1) {
        result_.warnings.push_back("skin " + std::to_string(i) + " has more than " +
                                   std::to_string(kMaxJointIndex + 1) + " joints: vertices of the extra ones follow the last supported joint");
      }
      for (cgltf_size j = 0; j < skin.joints_count; ++j) {
        out.joints.push_back(nodes[static_cast<std::size_t>(index_of(skin.joints[j], data_.nodes))]);
        std::array<float, 16> matrix = kIdentityMatrix;
        if (skin.inverse_bind_matrices != nullptr) {
          cgltf_accessor_read_float(skin.inverse_bind_matrices, j, matrix.data(), matrix.size());
        }
        out.inverse_bind.push_back(matrix);
      }
      result_.scene.skins.push_back(std::move(out));
    }
    for (NodeDesc& node : result_.scene.nodes) {
      if (node.source_node == kNoIndex) continue;
      node.skin = index_of(data_.nodes[node.source_node].skin, data_.skins);
    }
  }

  void load_animations() {
    const std::vector<int> nodes = node_indices();
    for (cgltf_size i = 0; i < data_.animations_count; ++i) {
      const cgltf_animation& animation = data_.animations[i];
      AnimationDesc out;
      out.name = animation.name ? animation.name : "animation_" + std::to_string(i);
      for (cgltf_size c = 0; c < animation.channels_count; ++c) {
        const cgltf_animation_channel& channel = animation.channels[c];
        if (channel.target_node == nullptr || channel.sampler == nullptr) continue;
        AnimationChannel track;
        switch (channel.target_path) {
          case cgltf_animation_path_type_translation: track.path = AnimationPath::Translation; break;
          case cgltf_animation_path_type_rotation: track.path = AnimationPath::Rotation; break;
          case cgltf_animation_path_type_scale: track.path = AnimationPath::Scale; break;
          default: continue;  // morph target weights are not supported
        }
        if (channel.sampler->interpolation == cgltf_interpolation_type_cubic_spline) {
          result_.warnings.push_back("animation '" + out.name + "': cubic spline keys are not supported, skipped");
          continue;
        }
        track.node = nodes[static_cast<std::size_t>(index_of(channel.target_node, data_.nodes))];
        if (track.node == kNoIndex) continue;
        track.times = unpack_floats(channel.sampler->input);
        track.values = unpack_floats(channel.sampler->output);
        if (track.times.empty()) continue;
        out.duration = std::max(out.duration, track.times.back());
        out.channels.push_back(std::move(track));
      }
      if (!out.channels.empty()) result_.scene.animations.push_back(std::move(out));
    }
  }

  static std::vector<std::uint8_t> to_rgba_bytes(const cgltf_accessor* accessor) {
    const std::vector<float> floats = unpack_floats(accessor);
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

  void load_nodes() {
    const cgltf_scene* scene = data_.scene != nullptr ? data_.scene
                               : (data_.scenes_count > 0 ? &data_.scenes[0] : nullptr);
    if (scene != nullptr) {
      for (cgltf_size i = 0; i < scene->nodes_count; ++i) add_node(*scene->nodes[i], kNoIndex);
    } else {
      for (cgltf_size i = 0; i < data_.nodes_count; ++i) {
        if (data_.nodes[i].parent == nullptr) add_node(data_.nodes[i], kNoIndex);
      }
    }
  }

  void add_node(const cgltf_node& node, int parent) {
    const int index = static_cast<int>(result_.scene.nodes.size());
    NodeDesc out;
    out.name = node.name ? node.name : "node_" + std::to_string(index);
    out.parent = parent;
    out.local = read_local_transform(node);
    out.mesh = index_of(node.mesh, data_.meshes);
    if (node.camera != nullptr && node.camera->type == cgltf_camera_type_perspective) {
      const auto& perspective = node.camera->data.perspective;
      out.camera = CameraDesc{perspective.yfov, perspective.znear,
                              perspective.has_zfar ? perspective.zfar : 0.0f};
    }
    if (node.light != nullptr) out.light = read_light(*node.light);
    out.source_node = index_of(&node, data_.nodes);
    out.source_camera = index_of(node.camera, data_.cameras);
    out.source_light = index_of(node.light, data_.lights);
    if (node.extras.data != nullptr) {
      ExtrasParseResult parsed = parse_extras(node.extras.data);
      if (!parsed.ok) {
        result_.warnings.push_back("node '" + out.name + "': bad extras (" + parsed.error + ")");
      }
      for (const std::string& key : parsed.skipped_keys) {
        result_.warnings.push_back("node '" + out.name + "': property '" + key + "' is not a scalar, ignored");
      }
      out.props = std::move(parsed.properties);
    }
    result_.scene.nodes.push_back(std::move(out));
    if (parent != kNoIndex) result_.scene.nodes[parent].children.push_back(index);
    for (cgltf_size c = 0; c < node.children_count; ++c) add_node(*node.children[c], index);
  }

  static LightDesc read_light(const cgltf_light& light) {
    LightDesc out;
    switch (light.type) {
      case cgltf_light_type_point: out.type = LightType::Point; break;
      case cgltf_light_type_spot: out.type = LightType::Spot; break;
      default: out.type = LightType::Directional; break;
    }
    out.color = {light.color[0], light.color[1], light.color[2], kOpaqueAlpha};
    out.intensity = light.intensity;
    out.range = light.range;
    return out;
  }

  const cgltf_data& data_;
  std::filesystem::path base_dir_;
  LoadResult& result_;
};

}  // namespace

LoadResult load_gltf_scene(const std::string& path) {
  LoadResult result;
  cgltf_options options{};
  cgltf_data* raw = nullptr;
  cgltf_result status = cgltf_parse_file(&options, path.c_str(), &raw);
  CgltfPtr data(raw);
  if (status != cgltf_result_success) {
    result.error = "cannot parse '" + path + "': " + describe(status);
    return result;
  }
  status = cgltf_load_buffers(&options, data.get(), path.c_str());
  if (status != cgltf_result_success) {
    result.error = "cannot load buffers of '" + path + "': " + describe(status);
    return result;
  }
  status = cgltf_validate(data.get());
  if (status != cgltf_result_success) {
    result.error = "invalid glTF '" + path + "': " + describe(status);
    return result;
  }
  Loader(*data, std::filesystem::path(path).parent_path(), result).run();
  result.ok = true;
  return result;
}

}  // namespace redgem
