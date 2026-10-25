#pragma once

// Pictures inside a .glb: added to its binary chunk, and pictures that were files next to the
// scene carried into it.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "editor/document/glb_file.hpp"

namespace redgem::glb_images {

// glTF's "mimeType" of a picture file's bytes (PNG, JPEG or WebP, by their first bytes); empty
// for anything else.
std::string mime_type_of(const std::vector<std::uint8_t>& bytes);

// Adds a picture to the file: its bytes into the binary chunk, a buffer view over them, and an
// image entry. Returns the image's number.
int append_image(GlbFile& glb, const std::vector<std::uint8_t>& bytes, const std::string& mime_type);

// Every picture the file names by "uri" (a file next to the scene, in `scene_dir`) is carried in
// its binary chunk instead, so that a copy of the file shows it wherever it goes. A picture that
// cannot be read there (missing, outside the folder, not a picture) is left without data.
void embed_linked_images(GlbFile& glb, const std::filesystem::path& scene_dir);

}  // namespace redgem::glb_images
