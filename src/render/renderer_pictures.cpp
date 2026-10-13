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

// Texts drawn by games (UI.text): rasterized at this height and scaled to the size asked for.
constexpr int kUiFontPixels = 72;
constexpr int kUiFirstLetter = 32;  // space
constexpr int kUiLastLetter = 255;  // through Latin-1: accented letters
constexpr const char* kFontFileType = ".ttf";

// The codes of the letters the game's font holds.
std::vector<int> ui_letters() {
  std::vector<int> letters;
  for (int code = kUiFirstLetter; code <= kUiLastLetter; ++code) letters.push_back(code);
  return letters;
}

// A picture that was looked for, or null for one that was not found.
const Texture2D* if_loaded(const Texture2D& picture) { return loaded(picture) ? &picture : nullptr; }

}  // namespace

void Renderer::Impl::load_ui_font() {
  const EmbeddedFile inter = embedded_ui_font();
  std::vector<int> letters = ui_letters();  // (not const: raylib takes them as plain int*)
  ui_font = LoadFontFromMemory(kFontFileType, inter.data, inter.size, kUiFontPixels, letters.data(),
                               static_cast<int>(letters.size()));
  if (!loaded(ui_font.texture)) {
    ui_font = GetFontDefault();
    return;
  }
  GenTextureMipmaps(&ui_font.texture);
  SetTextureFilter(ui_font.texture, TEXTURE_FILTER_TRILINEAR);
  owns_ui_font = true;
}

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
