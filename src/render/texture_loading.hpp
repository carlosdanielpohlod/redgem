#pragma once

// Pictures onto the graphics card: the images a scene carries and the files of a game's images
// folder. Nothing oversized or of another kind than PNG and JPEG is ever decoded.

#include <raylib.h>

#include <filesystem>
#include <string>

#include "scene/scene_desc.hpp"

namespace redgem::render_detail {

// A scene's image as a texture surfaces are drawn with, smaller copies of itself included. Its
// id is 0 when the scene left the image empty or it cannot be decoded (said in a warning).
Texture2D load_scene_texture(const ImageData& image);

// The first file of `folder` that `name` can mean (as it is, or with a picture's extension), as
// a texture. Its id is 0 when none does, when the name reaches outside the folder, or when the
// file is not a picture that is decoded.
Texture2D load_picture_file(const std::filesystem::path& folder, const std::string& name);

}  // namespace redgem::render_detail
