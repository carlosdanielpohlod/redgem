#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "editor/document/material_textures.hpp"

using namespace redgem;
using namespace redgem::material_textures;
using Json = nlohmann::json;

namespace {

constexpr float kBumpy = 0.5f;  // the normal map's scale in the document below

// Two pictures (the second a WebP one), a texture showing the first, and a material using it.
Json document() {
  return Json::parse(R"({
    "images": [{"bufferView": 0, "mimeType": "image/png"}, {"bufferView": 1, "mimeType": "image/webp"}],
    "textures": [{"source": 0, "sampler": 0}],
    "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0, "texCoord": 1}},
                   "normalTexture": {"index": 0, "scale": 0.5}}]
  })");
}

}  // namespace

TEST_CASE("every slot of a material description is reached, and its coat's pictures count too") {
  MaterialDesc material;
  int number = 0;
  for (const Slot slot : kSlots) image_in(material, slot) = number++;
  CHECK(material.base_color_image == 0);
  CHECK(material.metallic_roughness_image == 1);
  CHECK(material.normal_image == 2);
  CHECK(material.occlusion_image == 3);
  CHECK(material.emissive_image == 4);
  material.clearcoat.normal_image = 9;
  const std::vector<int> images = images_of(material);
  CHECK(images.size() == kSlots.size() + 3);
  CHECK(images.back() == 9);
}

TEST_CASE("a material's slots are read through its textures, the WebP picture first") {
  Json doc = document();
  CHECK(slot_image(doc, doc["materials"][0], Slot::BaseColor) == 0);
  CHECK(slot_image(doc, doc["materials"][0], Slot::Normal) == 0);
  CHECK(slot_image(doc, doc["materials"][0], Slot::Emissive) == kNoIndex);
  doc["textures"][0]["extensions"]["EXT_texture_webp"]["source"] = 1;
  CHECK(image_of_texture(doc, 0) == 1);
  CHECK(image_of_texture(doc, 5) == kNoIndex);
}

TEST_CASE("a texture showing a picture is found, or made") {
  Json doc = document();
  CHECK(texture_showing(doc, 0) == 0);
  CHECK(doc["textures"].size() == 1);
  // A WebP picture is shown through the extension that carries it, which the file then names.
  CHECK(texture_showing(doc, 1) == 1);
  CHECK(doc["textures"][1]["extensions"]["EXT_texture_webp"]["source"] == 1);
  CHECK(doc["extensionsUsed"] == Json::array({"EXT_texture_webp"}));
  CHECK(texture_showing(doc, 1) == 1);  // and found the next time
}

TEST_CASE("writing a material's slots changes only the ones whose picture changed") {
  Json doc = document();
  Json& material = doc["materials"][0];
  MaterialDesc desc;
  desc.base_color_image = 0;  // as it was: its texture coordinates stay
  desc.normal_image = 1;      // another picture: the slot keeps its scale
  desc.emissive_image = 0;    // a new slot
  write_slots(doc, material, desc);
  CHECK(material["pbrMetallicRoughness"]["baseColorTexture"] == Json({{"index", 0}, {"texCoord", 1}}));
  CHECK(slot_image(doc, material, Slot::Normal) == 1);
  CHECK(material["normalTexture"]["scale"] == kBumpy);
  CHECK(slot_image(doc, material, Slot::Emissive) == 0);
  CHECK(material["emissiveTexture"]["index"] == 0);

  desc.normal_image = kNoIndex;  // taken away
  write_slots(doc, material, desc);
  CHECK_FALSE(material.contains("normalTexture"));
  CHECK_FALSE(material.contains("occlusionTexture"));
}
