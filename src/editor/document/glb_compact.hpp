#pragma once

#include <optional>

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

}  // namespace redgem
