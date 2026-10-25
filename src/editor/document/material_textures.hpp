#pragma once

// The pictures a material is drawn with, as the editor sets them: which slots it has, where each
// one is in a glTF material, and how a slot is pointed at an image when the scene is written.
// Pure functions over JSON and descriptions, so each is tested by itself.

#include <array>
#include <vector>

#include <nlohmann/json.hpp>

#include "scene/scene_desc.hpp"

namespace redgem::material_textures {

// The texture slots the Inspector offers, in the order it lists them.
enum class Slot { BaseColor, MetallicRoughness, Normal, Occlusion, Emissive };
inline constexpr std::array<Slot, 5> kSlots = {Slot::BaseColor, Slot::MetallicRoughness, Slot::Normal, Slot::Occlusion,
                                               Slot::Emissive};

// The image a slot of a material shows (kNoIndex for none), to read and to change.
int image_in(const MaterialDesc& material, Slot slot);
int& image_in(MaterialDesc& material, Slot slot);
// Every image a material shows, in any slot (its coat's too).
std::vector<int> images_of(const MaterialDesc& material);

// The image a texture of the document shows, as the loader picks it (its WebP picture first);
// kNoIndex for none.
int image_of_texture(const nlohmann::json& doc, int texture);
// The image a slot of a material's JSON shows in the document (kNoIndex for none).
int slot_image(const nlohmann::json& doc, const nlohmann::json& material, Slot slot);
// A texture of the document that shows `image`: the first one that does, or a new one (a WebP
// picture through EXT_texture_webp, which the document then says it uses).
int texture_showing(nlohmann::json& doc, int image);
// Points the slots of a material's JSON at the images `desc` names. A slot that already shows its
// image is left as it is (its texture coordinates, scale and extensions with it); a slot without
// an image is taken out.
void write_slots(nlohmann::json& doc, nlohmann::json& material, const MaterialDesc& desc);

}  // namespace redgem::material_textures
