#pragma once

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "scene/scene_desc.hpp"

namespace redgem::tools::snapshot {

using Json = nlohmann::json;
using Fields = std::map<std::string, Json>;

// Caches shared asset fingerprints once; a node's fields contain identities, never file indices.
class SceneView {
 public:
  explicit SceneView(const SceneDesc& scene);
  Fields fields(int node) const;

 private:
  Json material_list(const NodeDesc& node) const;
  Json skeleton(const NodeDesc& node) const;
  void parts(Fields& fields, const NodeDesc& node) const;

  const SceneDesc& scene_;
  std::vector<std::string> images_;
  std::vector<std::string> meshes_;
  std::vector<Json> materials_;
  std::vector<Json> skins_;
  std::vector<Json> animations_;
};

bool property_field(const std::string& field);
bool same_field(const std::string& field, const Json& before, const Json& after);

}  // namespace redgem::tools::snapshot
