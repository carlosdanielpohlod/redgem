// The font the game's texts are drawn with and the pictures it asks for by name (UI.image, and
// what particles wear).

#include "render/renderer_impl.hpp"

#include <vector>

#include "core/log.hpp"
#include "render/texture_loading.hpp"
#include "render/ui_font.hpp"

namespace redgem {
namespace {

using namespace render_detail;

// A cached picture (including a placeholder), or null when GPU allocation failed.
const Texture2D* if_loaded(const Texture2D& picture) { return loaded(picture) ? &picture : nullptr; }

}  // namespace

void Renderer::Impl::load_ui_font() { ui_fonts.get("", ""); }

const Texture2D* Renderer::Impl::ui_image(const std::string& name) {
  const auto found = ui_images.find(name);
  if (found != ui_images.end()) return if_loaded(found->second);
  const Texture2D picture = load_picture_file(images_dir, name);
  if (loaded(picture)) {
    SetTextureFilter(picture, TEXTURE_FILTER_BILINEAR);
  } else {
    log::error("no picture named '" + name + "' in " + images_dir.string());
  }
  return if_loaded(ui_images.emplace(name, picture).first->second);
}

void Renderer::Impl::release_ui_images() {
  for (auto& [name, picture] : ui_images) {
    if (loaded(picture)) UnloadTexture(picture);
  }
  ui_images.clear();
}

}  // namespace redgem
