// The entries of the scene description an imported model brings, renumbered like its JSON.

#include "editor/document/model_import.hpp"

namespace redgem::model_import {
namespace {

int shifted(int index, int offset) { return index == kNoIndex ? kNoIndex : index + offset; }

}  // namespace

MaterialDesc imported_material(const MaterialDesc& original, const DescOffsets& offsets) {
  MaterialDesc material = original;
  for (int* image : {&material.base_color_image, &material.metallic_roughness_image, &material.normal_image,
                     &material.occlusion_image, &material.emissive_image}) {
    *image = shifted(*image, offsets.images);
  }
  return material;
}

MeshDesc imported_mesh(MeshDesc mesh, const DescOffsets& offsets) {
  for (PrimitiveDesc& primitive : mesh.primitives) primitive.material = shifted(primitive.material, offsets.materials);
  return mesh;
}

NodeDesc imported_node(NodeDesc node, const DescOffsets& offsets) {
  node.parent = node.parent == kNoIndex ? offsets.root : node.parent + offsets.nodes;
  for (int& child : node.children) child += offsets.nodes;
  node.mesh = shifted(node.mesh, offsets.meshes);
  node.skin = shifted(node.skin, offsets.skins);
  node.camera.reset();
  node.light.reset();
  node.source_camera = kNoIndex;
  node.source_light = kNoIndex;
  node.source_node = shifted(node.source_node, offsets.source_nodes);
  return node;
}

SkinDesc imported_skin(SkinDesc skin, const DescOffsets& offsets) {
  for (int& joint : skin.joints) joint = shifted(joint, offsets.nodes);
  return skin;
}

AnimationDesc imported_animation(AnimationDesc animation, const DescOffsets& offsets) {
  for (AnimationChannel& channel : animation.channels) channel.node += offsets.nodes;
  return animation;
}

}  // namespace redgem::model_import
