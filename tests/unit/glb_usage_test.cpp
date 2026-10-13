#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

#include "editor/glb_usage.hpp"
#include "editor/gltf_json.hpp"

using namespace redgem;
using namespace redgem::gltf_json;
using namespace redgem::glb_usage;
using nlohmann::json;

namespace {

constexpr int kDrawnMesh = 1;
constexpr int kSpareMesh = 0;

// Two meshes, of which the nodes draw only the second; each with its own material, texture,
// image, sampler, accessor and buffer view. A skeleton with its own accessor and view.
json two_meshes() {
  return {
      {kNodes, {{{kMesh, kDrawnMesh}}}},
      {kMeshes,
       {{{kPrimitives, {{{kAttributes, {{kPositionAttribute, kSpareMesh}}}, {kMaterial, kSpareMesh}}}}},
        {{kPrimitives, {{{kAttributes, {{kPositionAttribute, kDrawnMesh}}}, {kMaterial, kDrawnMesh}}}}}}},
      {kMaterials, {{{"baseColorTexture", {{kIndex, kSpareMesh}}}}, {{"baseColorTexture", {{kIndex, kDrawnMesh}}}}}},
      {kTextures, {{{kSource, kSpareMesh}, {kSampler, kSpareMesh}}, {{kSource, kDrawnMesh}, {kSampler, kDrawnMesh}}}},
      {kImages, {{{kBufferView, kSpareMesh}}, {{kBufferView, kDrawnMesh}}}},
      {kSamplers, json::array({json::object(), json::object()})},
      {kAccessors, {{{kBufferView, 2}}, {{kBufferView, 3}}, {{kBufferView, 4}}}},
      {kBufferViews, json::array({json::object(), json::object(), json::object(), json::object(), json::object()})},
      {kSkins, {{{kInverseBindMatrices, 2}}}}};
}

}  // namespace

TEST_CASE("the usage of a file follows what its nodes draw with, down to the bytes") {
  json doc = two_meshes();
  const Usage usage = usage_of(doc);
  CHECK(usage[kMeshKind] == std::vector<bool>{false, true});
  CHECK(usage[kMaterialKind] == std::vector<bool>{false, true});
  CHECK(usage[kTextureKind] == std::vector<bool>{false, true});
  CHECK(usage[kImageKind] == std::vector<bool>{false, true});
  CHECK(usage[kSamplerKind] == std::vector<bool>{false, true});
  CHECK(usage[kAccessorKind] == std::vector<bool>{false, true, true});  // the skeleton's too
  CHECK(usage[kBufferViewKind] == std::vector<bool>{false, true, false, true, true});
}

TEST_CASE("a file without nodes uses nothing, and one without lists has nothing to use") {
  json alone = two_meshes();
  alone.erase(kNodes);
  alone.erase(kSkins);
  for (const std::vector<bool>& used : usage_of(alone)) {
    CHECK(std::none_of(used.begin(), used.end(), [](bool one) { return one; }));
  }
  json empty = json::object();
  for (const std::vector<bool>& used : usage_of(empty)) CHECK(used.empty());
}

TEST_CASE("a reference that points at nothing, or an unknown extension, is not followed") {
  json doc = two_meshes();
  doc[kNodes][0][kMesh] = 2;
  CHECK_THROWS_AS(usage_of(doc), NotFollowed);
  json other = two_meshes();
  other[kExtensions]["KHR_other"] = json::object();
  CHECK_THROWS_AS(usage_of(other), NotFollowed);
  json lights = two_meshes();
  lights[kExtensions][kLightsExtension] = json::object();
  CHECK_NOTHROW(usage_of(lights));
}
