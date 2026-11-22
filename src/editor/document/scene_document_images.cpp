// Part of SceneDocument: pictures added for materials to show (a texture slot set in the
// Inspector), carried inside the scene's file.

#include "editor/document/scene_document.hpp"

#include "core/image_header.hpp"
#include "core/import_settings.hpp"
#include "core/safe_path.hpp"
#include "editor/document/glb_images.hpp"
#include "editor/document/picture_fit.hpp"
#include "editor/document/gltf_json.hpp"
#include "scene/gltf/gltf_checks.hpp"

namespace redgem {
namespace {

using namespace gltf_json;

constexpr const char* kPngExtension = ".png";  // the file kinds the image decoder is told
constexpr const char* kJpegExtension = ".jpg";
constexpr const char* kWebpExtension = ".webp";

const char* extension_for(const std::string& mime_type) {
  if (mime_type == kJpegMime) return kJpegExtension;
  return mime_type == kWebpMime ? kWebpExtension : kPngExtension;
}

// A picture the engine can show: PNG, JPEG or still WebP, not too big to decode.
bool showable(const std::vector<std::uint8_t>& bytes) { return decodable_image(bytes.data(), bytes.size()); }

std::optional<std::vector<std::uint8_t>> not_carried(std::string& error, const std::string& why) {
  error = why;
  return std::nullopt;
}

// The bytes of a picture file as the scene carries them: one the engine can show, no bigger than
// its import settings' max_size.
std::optional<std::vector<std::uint8_t>> carried_picture(const std::filesystem::path& file, std::string& error) {
  const std::string name = printable(file.filename().string());
  const auto bytes = read_file_up_to(file, gltf_checks::kMaxImageFileBytes);
  if (!bytes) return not_carried(error, "cannot read " + name);
  if (!showable(*bytes))
    return not_carried(error, name + " is not a PNG, JPEG or still WebP picture the engine can show");
  auto fitted = picture_fit::fitted_picture(*bytes, import_settings::texture_settings(file).max_size);
  if (!fitted) return not_carried(error, name + " cannot be scaled down to the max_size of its import settings");
  return fitted;
}

}  // namespace

int SceneDocument::add_image(const std::filesystem::path& file, std::string& error) {
  const auto fitted = carried_picture(file, error);
  if (!fitted) return kNoIndex;
  const int known = image_with(*fitted);
  if (known != kNoIndex) return hold_unshown(known);
  const std::string mime_type = glb_images::mime_type_of(*fitted);
  const int image = glb_images::append_image(glb_, *fitted, mime_type, file.filename().string());
  // (The description lists the file's images in the same order: one it lacks is an empty one.)
  if (desc_.images.size() < static_cast<std::size_t>(image)) desc_.images.resize(static_cast<std::size_t>(image));
  desc_.images.push_back(ImageData{*fitted, extension_for(mime_type)});
  return hold_unshown(image);
}

int SceneDocument::hold_unshown(int image) {
  // (Until a slot shows it, nothing the memory of undo looks at knows of it.)
  unshown_images_.insert(image);
  return image;
}

int SceneDocument::image_with(const std::vector<std::uint8_t>& bytes) const {
  for (std::size_t image = 0; image < desc_.images.size(); ++image) {
    if (desc_.images[image].encoded == bytes) return static_cast<int>(image);
  }
  return kNoIndex;
}

std::string SceneDocument::image_name(int image) const {
  const nlohmann::json* entry = entry_at(glb_.json, kImages, image);
  if (entry == nullptr) return {};
  const std::string name = text_at(*entry, kName);
  return name.empty() ? text_at(*entry, kUri) : name;
}

}  // namespace redgem
