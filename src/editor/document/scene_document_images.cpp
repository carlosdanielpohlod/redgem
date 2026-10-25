// Part of SceneDocument: pictures added for materials to show (a texture slot set in the
// Inspector), carried inside the scene's file.

#include "editor/document/scene_document.hpp"

#include "core/image_header.hpp"
#include "core/safe_path.hpp"
#include "editor/document/glb_images.hpp"
#include "editor/document/gltf_json.hpp"
#include "scene/gltf/gltf_checks.hpp"

namespace redgem {
namespace {

using namespace gltf_json;

constexpr const char* kPngExtension = ".png";  // the file kinds the image decoder is told
constexpr const char* kJpegExtension = ".jpg";
constexpr const char* kWebpExtension = ".webp";
constexpr const char* kJpegMime = "image/jpeg";
constexpr const char* kWebpMime = "image/webp";

const char* extension_for(const std::string& mime_type) {
  if (mime_type == kJpegMime) return kJpegExtension;
  return mime_type == kWebpMime ? kWebpExtension : kPngExtension;
}

int refused(std::string& error, const std::string& why) {
  error = why;
  return kNoIndex;
}

// A picture the engine can show: PNG, JPEG or still WebP, not too big to decode.
bool showable(const std::vector<std::uint8_t>& bytes) { return decodable_image(bytes.data(), bytes.size()); }

}  // namespace

int SceneDocument::add_image(const std::filesystem::path& file, std::string& error) {
  const std::string name = printable(file.filename().string());
  const auto bytes = read_file_up_to(file, gltf_checks::kMaxImageFileBytes);
  if (!bytes) return refused(error, "cannot read " + name);
  if (!showable(*bytes)) return refused(error, name + " is not a PNG, JPEG or still WebP picture the engine can show");
  const int known = image_with(*bytes);
  if (known != kNoIndex) return known;
  const std::string mime_type = glb_images::mime_type_of(*bytes);
  const int image = glb_images::append_image(glb_, *bytes, mime_type, file.filename().string());
  // (The description lists the file's images in the same order: one it lacks is an empty one.)
  if (desc_.images.size() < static_cast<std::size_t>(image)) desc_.images.resize(static_cast<std::size_t>(image));
  desc_.images.push_back(ImageData{*bytes, extension_for(mime_type)});
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
  for (const char* key : {kName, kUri}) {
    const auto text = entry->find(key);
    if (text != entry->end() && text->is_string()) return text->get<std::string>();
  }
  return {};
}

}  // namespace redgem
