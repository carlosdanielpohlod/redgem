#include "editor/document/gltf_json.hpp"

#include <algorithm>

namespace redgem::gltf_json {
namespace {

constexpr int kFirstScene = 0;  // the scene a file loads when it does not say which

bool whole_and_not_negative(const nlohmann::json& value) {
  return value.is_number_integer() && value.get<std::int64_t>() >= 0;
}

// Whether an animation channel says which node it moves.
bool names_node(const nlohmann::json& channel) { return channel.contains(kTarget) && channel[kTarget].contains(kNode); }

}  // namespace

const nlohmann::json* entry_at(const nlohmann::json& json, const char* key, const nlohmann::json& index) {
  const std::optional<int> number = index_from(index);
  if (!number) return nullptr;
  if (!has_list(json, key)) return nullptr;
  const nlohmann::json& list = json[key];
  if (!in_range(*number, list.size())) return nullptr;
  const nlohmann::json& entry = list[static_cast<std::size_t>(*number)];
  return entry.is_object() ? &entry : nullptr;
}

std::optional<std::size_t> size_from(const nlohmann::json& value) {
  if (value.is_number_unsigned()) return value.get<std::size_t>();
  if (!whole_and_not_negative(value)) return std::nullopt;
  return value.get<std::size_t>();
}

std::optional<int> index_from(const nlohmann::json& value) {
  const std::size_t size = size_from(value).value_or(kMaxIndex + 1);
  if (size > kMaxIndex) return std::nullopt;
  return static_cast<int>(size);
}

nlohmann::json list_of(const nlohmann::json& json, const char* key) { return json.value(key, nlohmann::json::array()); }

nlohmann::json& array_at(nlohmann::json& json, const char* key) {
  nlohmann::json& list = json[key];
  if (!list.is_array()) list = nlohmann::json::array();
  return list;
}

int last_index(const nlohmann::json& list) { return static_cast<int>(list.size()) - 1; }

bool has_list(const nlohmann::json& json, const char* key) { return json.contains(key) && json[key].is_array(); }

bool has_object(const nlohmann::json& json, const char* key) { return json.contains(key) && json[key].is_object(); }

nlohmann::json empty_document() {
  return {{kAsset, {{kVersion, kGltfVersion}, {kGenerator, kGeneratorName}}},
          {kScene, kFirstScene},
          {kScenes, nlohmann::json::array({{{kName, kDefaultSceneName}, {kNodes, nlohmann::json::array()}}})},
          {kNodes, nlohmann::json::array()}};
}

void add_extension_used(nlohmann::json& json, const char* extension) {
  nlohmann::json& used = json[kExtensionsUsed];
  if (!used.is_array()) used = nlohmann::json::array();
  if (std::find(used.begin(), used.end(), extension) == used.end()) used.push_back(extension);
}

void remove_extension_used(nlohmann::json& json, const char* extension) {
  for (const char* list : {kExtensionsUsed, kExtensionsRequired}) {
    nlohmann::json kept = nlohmann::json::array();
    for (const nlohmann::json& name : list_of(json, list)) {
      if (name != extension) kept.push_back(name);
    }
    set_unless_empty(json, list, kept);
  }
}

void set_unless_empty(nlohmann::json& object, const char* key, const nlohmann::json& value) {
  if (value.empty()) {
    object.erase(key);
  } else {
    object[key] = value;
  }
}

void set_or_erase_index(nlohmann::json& object, const char* key, int index) {
  if (index == kNoIndex) {
    object.erase(key);
  } else {
    object[key] = index;
  }
}

int current_index(const std::vector<int>& current, int source) {
  return source >= 0 && source < static_cast<int>(current.size()) ? current[source] : kNoIndex;
}

std::optional<nlohmann::json> skin_with_current_nodes(nlohmann::json skin, const std::vector<int>& current) {
  bool intact = true;
  for (nlohmann::json& joint : skin[kJoints]) {
    const int index = current_index(current, index_from(joint).value_or(kNoIndex));
    intact = intact && index != kNoIndex;
    joint = index;
  }
  if (!intact) return std::nullopt;
  if (!skin.contains(kSkeleton)) return skin;
  const int root = current_index(current, index_from(skin[kSkeleton]).value_or(kNoIndex));
  set_or_erase_index(skin, kSkeleton, root);
  return skin;
}

std::optional<nlohmann::json> animation_with_current_nodes(nlohmann::json animation, const std::vector<int>& current) {
  nlohmann::json channels = nlohmann::json::array();
  for (nlohmann::json channel : animation[kChannels]) {
    if (!names_node(channel)) continue;
    const int index = current_index(current, index_from(channel[kTarget][kNode]).value_or(kNoIndex));
    if (index == kNoIndex) continue;
    channel[kTarget][kNode] = index;
    channels.push_back(std::move(channel));
  }
  if (channels.empty()) return std::nullopt;
  animation[kChannels] = channels;
  return animation;
}

bool has_several_scenes(const nlohmann::json& json) { return json.contains(kScenes) && json[kScenes].size() > 1; }

void keep_active_scene(nlohmann::json& json, const nlohmann::json& roots) {
  // (The file's own "scene" may be anything: a number that names none of its scenes, or no number.)
  const nlohmann::json* active = entry_at(json, kScenes, json.value(kScene, nlohmann::json(kFirstScene)));
  nlohmann::json scene = active != nullptr ? *active : nlohmann::json::object();
  scene[kNodes] = roots;
  json[kScenes] = nlohmann::json::array({scene});
  json[kScene] = kFirstScene;
}

}  // namespace redgem::gltf_json
