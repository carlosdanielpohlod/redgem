#pragma once

// Data a scene's file holds twice (a model imported twice, a picture used by two pasted objects):
// found by content, so that it is written once.

#include <cstdint>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

namespace redgem {

// The file's JSON with every reference to a repeated entry pointed at the first entry of its list
// that holds the same: buffer views with the same bytes (pictures and vertices alike), then
// accessors, images, samplers and textures whose JSON is the same once their own references are
// merged. The repeats stay in their lists, pointed at by nothing: without_unused leaves them out.
// Materials and meshes are never merged: each keeps the identity the Inspector edits it by. The
// accessors and buffer views of a terrain's mesh are neither merged nor shared, since the editor
// writes reshaped vertices over them.
// nullopt when nothing repeats, and when the file points at its data in a way that is not followed
// here (see glb_usage).
std::optional<nlohmann::json> with_repeats_merged(const nlohmann::json& json, const std::vector<std::uint8_t>& bin);

}  // namespace redgem
