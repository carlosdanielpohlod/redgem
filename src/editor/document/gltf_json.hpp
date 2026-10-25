#pragma once

// The glTF JSON the editor writes when it saves a scene. Everything here works on JSON values
// alone (no document, no file), so each piece can be tested by itself.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "scene/scene_desc.hpp"

namespace redgem::gltf_json {

// Field names of glTF 2.0.
inline constexpr const char* kAccessors = "accessors";
inline constexpr const char* kAnimations = "animations";
inline constexpr const char* kAsset = "asset";
inline constexpr const char* kAttributes = "attributes";
inline constexpr const char* kBaseColorFactor = "baseColorFactor";
inline constexpr const char* kBuffer = "buffer";
inline constexpr const char* kBufferView = "bufferView";
inline constexpr const char* kBufferViews = "bufferViews";
inline constexpr const char* kBuffers = "buffers";
inline constexpr const char* kByteLength = "byteLength";
inline constexpr const char* kByteOffset = "byteOffset";
inline constexpr const char* kByteStride = "byteStride";
inline constexpr const char* kCamera = "camera";
inline constexpr const char* kCameras = "cameras";
inline constexpr const char* kChannels = "channels";
inline constexpr const char* kChildren = "children";
inline constexpr const char* kColor = "color";
inline constexpr const char* kComponentType = "componentType";
inline constexpr const char* kCount = "count";
inline constexpr const char* kEmissiveFactor = "emissiveFactor";
inline constexpr const char* kEmissiveStrength = "emissiveStrength";
inline constexpr const char* kExtensions = "extensions";
inline constexpr const char* kExtensionsRequired = "extensionsRequired";
inline constexpr const char* kExtensionsUsed = "extensionsUsed";
inline constexpr const char* kExtras = "extras";
inline constexpr const char* kGenerator = "generator";
inline constexpr const char* kImages = "images";
inline constexpr const char* kIndex = "index";
inline constexpr const char* kIndices = "indices";
inline constexpr const char* kInput = "input";
inline constexpr const char* kIntensity = "intensity";
inline constexpr const char* kInverseBindMatrices = "inverseBindMatrices";
inline constexpr const char* kJoints = "joints";
inline constexpr const char* kLight = "light";
inline constexpr const char* kLights = "lights";
inline constexpr const char* kMaterial = "material";
inline constexpr const char* kMaterials = "materials";
inline constexpr const char* kMatrix = "matrix";
inline constexpr const char* kMax = "max";
inline constexpr const char* kMesh = "mesh";
inline constexpr const char* kMeshes = "meshes";
inline constexpr const char* kMetallicFactor = "metallicFactor";
inline constexpr const char* kMin = "min";
inline constexpr const char* kName = "name";
inline constexpr const char* kNode = "node";
inline constexpr const char* kNodes = "nodes";
inline constexpr const char* kOutput = "output";
inline constexpr const char* kPbr = "pbrMetallicRoughness";
inline constexpr const char* kPerspective = "perspective";
inline constexpr const char* kPrimitives = "primitives";
inline constexpr const char* kRange = "range";
inline constexpr const char* kRotation = "rotation";
inline constexpr const char* kRoughnessFactor = "roughnessFactor";
inline constexpr const char* kSampler = "sampler";
inline constexpr const char* kSamplers = "samplers";
inline constexpr const char* kScale = "scale";
inline constexpr const char* kScene = "scene";
inline constexpr const char* kScenes = "scenes";
inline constexpr const char* kSkeleton = "skeleton";
inline constexpr const char* kSkin = "skin";
inline constexpr const char* kSkins = "skins";
inline constexpr const char* kSource = "source";
inline constexpr const char* kSparse = "sparse";
inline constexpr const char* kTarget = "target";
inline constexpr const char* kTargetNames = "targetNames";
inline constexpr const char* kTargets = "targets";
inline constexpr const char* kTextures = "textures";
inline constexpr const char* kTranslation = "translation";
inline constexpr const char* kType = "type";
inline constexpr const char* kUri = "uri";
inline constexpr const char* kValues = "values";
inline constexpr const char* kVersion = "version";
inline constexpr const char* kWeights = "weights";
inline constexpr const char* kYfov = "yfov";
inline constexpr const char* kZfar = "zfar";
inline constexpr const char* kZnear = "znear";

// Vertex streams of a mesh, and how an accessor says what its elements are.
inline constexpr const char* kPositionAttribute = "POSITION";
inline constexpr const char* kNormalAttribute = "NORMAL";
inline constexpr const char* kTexcoordAttribute = "TEXCOORD_0";
inline constexpr const char* kVec3Type = "VEC3";
inline constexpr const char* kVec2Type = "VEC2";
inline constexpr const char* kScalarType = "SCALAR";

// What else an accessor and a buffer view say.
inline constexpr int kFloatComponent = 5126;  // glTF componentType FLOAT
inline constexpr int kOnlyBuffer = 0;         // a .glb keeps everything in its first buffer
// The highest number taken as an index into one of the file's lists (far more entries than a file holds).
inline constexpr std::size_t kMaxIndex = 1u << 30;

// Values of "type" on lights and cameras.
inline constexpr const char* kDirectionalLight = "directional";
inline constexpr const char* kPointLight = "point";
inline constexpr const char* kSpotLight = "spot";
inline constexpr const char* kPerspectiveCamera = "perspective";

// Extensions the editor writes.
inline constexpr const char* kLightsExtension = "KHR_lights_punctual";
inline constexpr const char* kClearcoatExtension = "KHR_materials_clearcoat";
inline constexpr const char* kClearcoatNormalTexture = "clearcoatNormalTexture";
inline constexpr const char* kClearcoatFactor = "clearcoatFactor";
inline constexpr const char* kClearcoatRoughnessFactor = "clearcoatRoughnessFactor";
inline constexpr const char* kEmissiveStrengthExtension = "KHR_materials_emissive_strength";

// What a document made here says about itself.
inline constexpr const char* kGltfVersion = "2.0";
inline constexpr const char* kGeneratorName = "RedGem";
inline constexpr const char* kDefaultSceneName = "Scene";

// A scene with no objects yet.
nlohmann::json empty_document();

// What a file gives as an index into one of its lists: a whole number, zero or more and not
// absurdly large. nullopt for anything else (a text, a fraction, a negative number): a file may
// hold any JSON where glTF wants an index.
std::optional<int> index_from(const nlohmann::json& value);
// A whole number of zero or more as a size; nullopt for anything else. (Numbers read from a file
// and numbers the editor wrote itself are kept differently by the JSON library: both are taken.)
std::optional<std::size_t> size_from(const nlohmann::json& value);
// Whether `index` names one of `entries` entries.
inline bool in_range(int index, std::size_t entries) { return index >= 0 && index < static_cast<int>(entries); }

// object[key] when the object has it and it is a text; "" otherwise.
std::string text_at(const nlohmann::json& object, const char* key);

// The file's lists.
// json[key][index], when the file has such an entry and it is an object; nullptr otherwise.
const nlohmann::json* entry_at(const nlohmann::json& json, const char* key, const nlohmann::json& index);
// json[key] as a list (empty when the file has none).
nlohmann::json list_of(const nlohmann::json& json, const char* key);
// json[key], made a list when it is not one yet.
nlohmann::json& array_at(nlohmann::json& json, const char* key);
int last_index(const nlohmann::json& list);
// Whether json[key] is there, and a list or an object.
bool has_list(const nlohmann::json& json, const char* key);
bool has_object(const nlohmann::json& json, const char* key);

// A custom property as glTF extras hold it (a flag, a number or a text).
nlohmann::json property_json(const PropertyValue& value);

// The node as written on save: its original glTF JSON (unknown fields such as weights or other
// extensions survive) with the editable fields replaced.
nlohmann::json node_json(const NodeDesc& node, const nlohmann::json& original);

// The material as written on save, on top of its original JSON (textures and unknown fields stay).
nlohmann::json material_json(const MaterialDesc& material, const nlohmann::json& original);
// The file's materials with the edited ones written over them; materials made in the editor are
// added after the ones the file had.
nlohmann::json materials_json(nlohmann::json existing, const std::vector<MaterialDesc>& materials);
// How far above glTF's limit of 1 a material glows (1 when it does not), and whether that takes
// the emissive strength extension to write.
float emissive_strength(const MaterialDesc& material);
bool needs_emissive_strength(const MaterialDesc& material);

// Lists `extension` in the document's "extensionsUsed" once.
void add_extension_used(nlohmann::json& json, const char* extension);
// Takes `extension` out of the document's "extensionsUsed" and "extensionsRequired" (and each
// list, once it is empty).
void remove_extension_used(nlohmann::json& json, const char* extension);

// Sets `key` to `value`, or removes the key when the value is an empty list or object.
void set_unless_empty(nlohmann::json& object, const char* key, const nlohmann::json& value);
// Sets `key` to `index`, or removes the key for kNoIndex.
void set_or_erase_index(nlohmann::json& object, const char* key, int index);

const char* light_type_name(LightType type);
// Writes a light's or a camera's settings over its entry, keeping the entry's other fields.
void write_light(nlohmann::json& entry, const LightDesc& light);
void write_camera(nlohmann::json& entry, const CameraDesc& camera);

// Gives a node an entry of its own in `entries` (the file's lights or cameras): the one it came
// from when no other node took it yet, else a copy of it (a duplicate), else a new empty one.
// Returns the entry's index and remembers it in `claimed`.
int claim_entry(nlohmann::json& entries, int source, std::set<int>& claimed);

// Where the node that was number `source` when it was loaded is now (`current` comes from
// SceneDocument: kNoIndex for deleted nodes and for numbers it never had).
int current_index(const std::vector<int>& current, int source);
// A skin pointing at its joints' current numbers; nullopt when one of its joints is gone.
std::optional<nlohmann::json> skin_with_current_nodes(nlohmann::json skin, const std::vector<int>& current);
// An animation pointing at its nodes' current numbers, without the channels of deleted nodes;
// nullopt when no channel is left.
std::optional<nlohmann::json> animation_with_current_nodes(nlohmann::json animation, const std::vector<int>& current);

// True when the file holds more scenes than the one the engine loads.
bool has_several_scenes(const nlohmann::json& json);
// Makes the scene the engine loads the file's only one, with `roots` as its top-level nodes.
void keep_active_scene(nlohmann::json& json, const nlohmann::json& roots);

}  // namespace redgem::gltf_json
