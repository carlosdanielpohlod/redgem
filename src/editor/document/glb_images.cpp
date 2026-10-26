#include "editor/document/glb_images.hpp"

#include <optional>

#include "core/image_header.hpp"
#include "core/safe_path.hpp"
#include "editor/document/glb_geometry.hpp"
#include "editor/document/gltf_json.hpp"
#include "scene/gltf/gltf_checks.hpp"

namespace redgem::glb_images {
namespace {

using namespace gltf_json;

// The picture file `uri` names, as the scene in `scene_dir` reads it; nullopt when it names none
// the scene may read (only files inside its own folder, see core/safe_path.hpp).
std::optional<std::vector<std::uint8_t>> linked_picture(const std::string& uri,
                                                        const std::filesystem::path& scene_dir) {
  const std::optional<std::filesystem::path> file = file_inside(scene_dir, uri);
  if (!file) return std::nullopt;
  return read_file_up_to(*file, gltf_checks::kMaxImageFileBytes);
}

// The file an image names by its "uri" (not a picture written into the JSON itself); "" for none.
std::string linked_file(const nlohmann::json& image) {
  const std::string uri = text_at(image, kUri);
  return gltf_checks::names_a_file(uri.c_str()) ? uri : std::string();
}

// The bytes added to the binary chunk, with a buffer view over them. Returns the view's number.
int append_view(GlbFile& glb, const std::vector<std::uint8_t>& bytes) {
  const std::size_t offset = glb_geometry::append_binary(glb, bytes.data(), bytes.size());
  nlohmann::json& views = array_at(glb.json, kBufferViews);
  views.push_back({{kBuffer, kOnlyBuffer}, {kByteOffset, offset}, {kByteLength, bytes.size()}});
  return last_index(views);
}

// The image points at bytes added to the binary chunk instead of at its file.
void carry_inside(GlbFile& glb, nlohmann::json& image, const std::vector<std::uint8_t>& bytes) {
  image.erase(kUri);
  image[kBufferView] = append_view(glb, bytes);
  image[kMimeType] = mime_type_of(bytes);
}

// One image of the file carried in, when it names a picture file the scene reads.
void embed_linked_image(GlbFile& glb, nlohmann::json& image, const std::filesystem::path& scene_dir) {
  const std::string file = image.is_object() ? linked_file(image) : std::string();
  if (file.empty()) return;
  const std::optional<std::vector<std::uint8_t>> bytes = linked_picture(file, scene_dir);
  if (!bytes) return;
  if (!mime_type_of(*bytes).empty()) carry_inside(glb, image, *bytes);  // (a picture glTF can name)
}

}  // namespace

std::string mime_type_of(const std::vector<std::uint8_t>& bytes) {
  const std::optional<EncodedImage> image = EncodedImage::read(bytes.data(), bytes.size());
  if (!image) return {};
  if (image->format() == EncodedImageFormat::Png) return kPngMime;
  return image->format() == EncodedImageFormat::Jpeg ? kJpegMime : kWebpMime;
}

int append_image(GlbFile& glb, const std::vector<std::uint8_t>& bytes, const std::string& mime_type,
                 const std::string& name) {
  const int view = append_view(glb, bytes);
  nlohmann::json& images = array_at(glb.json, kImages);
  images.push_back({{kName, name}, {kBufferView, view}, {kMimeType, mime_type}});
  return last_index(images);
}

void embed_linked_images(GlbFile& glb, const std::filesystem::path& scene_dir) {
  if (!has_list(glb.json, kImages)) return;
  // (Carrying one in adds buffer views, not images: the list walked stays as it is.)
  for (nlohmann::json& image : glb.json[kImages]) embed_linked_image(glb, image, scene_dir);
}

}  // namespace redgem::glb_images
