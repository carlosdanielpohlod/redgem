#include "app/splash_screen.hpp"

#include <raylib.h>

#include <cstdint>
#include <vector>

#include "app/game_identity.hpp"
#include "app/recording.hpp"
#include "app/window_presenter.hpp"
#include "core/image_header.hpp"
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "core/translations.hpp"

namespace redgem {
namespace {

constexpr const char* kPngFileType = ".png";  // what raylib is told the bytes are
constexpr ::Color kBars{0, 0, 0, 255};        // around a picture of another shape than the window
constexpr ::Color kUntinted{255, 255, 255, 255};

// What the picture's file holds, when it can be decoded as a picture.
std::optional<std::vector<std::uint8_t>> picture_bytes(const std::filesystem::path& file) {
  auto bytes = read_file_up_to(file, kMaxEncodedImageBytes);
  if (bytes && decodable_image(bytes->data(), bytes->size())) return bytes;
  return std::nullopt;
}

void draw_fitted(const Texture2D& picture) {
  const Letterbox box =
      letterbox(FrameSize{GetScreenWidth(), GetScreenHeight()}, FrameSize{picture.width, picture.height});
  const Rectangle whole{0.0f, 0.0f, static_cast<float>(picture.width), static_cast<float>(picture.height)};
  BeginDrawing();
  ClearBackground(kBars);
  DrawTexturePro(picture, whole, Rectangle{box.x, box.y, box.width, box.height}, Vector2{0.0f, 0.0f}, 0.0f, kUntinted);
  EndDrawing();
}

// Draws the picture once: it stays on the window until the game's first frame replaces it.
void show_picture(const std::vector<std::uint8_t>& bytes) {
  Image image = LoadImageFromMemory(kPngFileType, bytes.data(), static_cast<int>(bytes.size()));
  if (image.data == nullptr) return;
  const Texture2D picture = LoadTextureFromImage(image);
  UnloadImage(image);
  draw_fitted(picture);
  UnloadTexture(picture);
}

}  // namespace

std::optional<std::filesystem::path> splash_picture(const std::filesystem::path& game_dir) {
  const GameIdentity identity = read_game_identity(game_dir).identity;
  if (identity.splash.empty()) return std::nullopt;
  auto file = file_inside(game_dir, identity.splash);
  if (file && picture_bytes(*file)) return file;
  log::warning(tr("game.splash.unusable", {{"picture", printable(identity.splash.generic_string())}}));
  return std::nullopt;
}

void show_splash(Presenter* presenter, const std::filesystem::path& game_dir) {
  if (dynamic_cast<WindowPresenter*>(presenter) == nullptr) return;
  const auto file = splash_picture(game_dir);
  if (!file) return;
  if (const auto bytes = picture_bytes(*file)) show_picture(*bytes);
}

}  // namespace redgem
