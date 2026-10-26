#pragma once

#include <optional>
#include <set>
#include <vector>

#include "editor/document/glb_file.hpp"

namespace redgem {

// A scene's file without what nothing in it uses any more: the meshes no object shows (of
// objects that were deleted, of an import that was undone), the materials, textures and pictures
// only those used, the bytes all of them kept in the binary chunk, and the lights and cameras of
// deleted objects. What is left is numbered
// anew, and everything that points at it follows.
// nullopt when there is nothing to leave out, and when the file points at its data in a way that
// is not followed here (an extension with indices of its own): such a file is saved whole.
std::optional<GlbFile> without_unused(const GlbFile& glb);
// The same for a file given as its JSON and its binary chunk apart (the chunk may be another
// file's, as long as the JSON points into it): only the bytes kept are copied.
std::optional<GlbFile> without_unused(const nlohmann::json& json, const std::vector<std::uint8_t>& bin);

// What the editor keeps of a file's objects apart from its JSON (see SceneDocument): every node,
// skeleton and animation as loaded or imported.
struct GlbSources {
  const nlohmann::json& nodes;
  const nlohmann::json& skins;
  const nlohmann::json& animations;
};

// For a scene's file kept in memory while it is edited, where nothing may be numbered anew: the
// bytes only the meshes `released` (by number) reached leave the binary chunk. The buffer views
// that held them keep their length and point at the chunk's start (nothing reads them, and the
// file stays whole however it is written). Returns which pictures something still uses, or
// nullopt, with the file as it was, when it points at its data in a way not followed here.
// `held_images` (by number) stay with their bytes whatever reaches them: pictures the editor knows
// to be used before the JSON says so.
std::optional<std::vector<bool>> release_meshes(GlbFile& glb, const std::set<int>& released, const GlbSources& sources,
                                                const std::vector<bool>& held_images = {});

}  // namespace redgem
