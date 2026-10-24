#pragma once

// Pictures onto the graphics card: the images a scene carries and the files of a game's images
// folder. Nothing oversized or of another kind than PNG and JPEG is ever decoded.

#include <raylib.h>

#include <filesystem>
#include <string>

#include "scene/scene_desc.hpp"

namespace redgem::render_detail {

// A scene's image as a texture surfaces are drawn with, smaller copies of itself included. Its
// missing or undecodable data becomes an opaque checkerboard, with a warning.
Texture2D load_scene_texture(const ImageData& image);

// The first file of `folder` that `name` can mean (as it is, or with a picture's extension), as
// a texture. Missing, refused and undecodable files become an opaque checkerboard, with a warning.
// Every result is independently owned by the caller, including placeholders.
Texture2D load_picture_file(const std::filesystem::path& folder, const std::string& name);

}  // namespace redgem::render_detail
