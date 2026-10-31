#include "scene/camera_ray.hpp"

#include <cmath>

#include "core/letterbox.hpp"
#include "scene/scene_config.hpp"

namespace redgem {
namespace {

constexpr float kFullSpan = 2.0f;  // a picture spans -1..1 across and up
constexpr float kHalfSpan = 1.0f;

// Where the world's picture lies on the screen, in pixels, and how wide it is for its height.
struct PictureBox {
  Letterbox shown;
  float aspect = 1.0f;
};

// A point of the picture from -1 to 1 across it (right positive) and up it (up positive).
struct Centered {
  float across = 0.0f;
  float upward = 0.0f;
};

// Half the picture's width and height: per meter in front of a perspective lens, in meters for an
// orthographic one.
struct Spread {
  float across = 0.0f;
  float upward = 0.0f;
};

bool has_picture_size(const ScreenFrame& frame) { return frame.picture_width > 0 && frame.picture_height > 0; }

bool is_orthographic(const Lens& lens) { return lens.ortho_height > 0.0f; }

float fov_of(const Lens& lens) {
  return lens.fov_y_radians > 0.0f ? lens.fov_y_radians : degrees_to_radians(config::kDefaultFovYDegrees);
}

PictureBox picture_box(const ScreenFrame& frame) {
  const float screen_width = static_cast<float>(frame.screen_width);
  const float screen_height = static_cast<float>(frame.screen_height);
  const bool sized = has_picture_size(frame);
  const float picture_width = sized ? static_cast<float>(frame.picture_width) : screen_width;
  const float picture_height = sized ? static_cast<float>(frame.picture_height) : screen_height;
  // (As the window shows a picture drawn at another size.)
  return {fit_picture(screen_width, screen_height, picture_width, picture_height), picture_width / picture_height};
}

Spread spread_of(const Lens& lens, float aspect) {
  const float half_height = is_orthographic(lens) ? lens.ortho_height * kHalf : std::tan(fov_of(lens) * kHalf);
  return {half_height * aspect, half_height};
}

Centered centered(const Letterbox& box, ScreenPoint point) {
  return {(point.x - box.x) / box.width * kFullSpan - kHalfSpan,
          kHalfSpan - (point.y - box.y) / box.height * kFullSpan};
}

ScreenPoint on_screen(const Letterbox& box, Centered point) {
  return {box.x + (point.across + kHalfSpan) / kFullSpan * box.width,
          box.y + (kHalfSpan - point.upward) / kFullSpan * box.height};
}

// Whether a point is in front of the camera, past its near plane (NaN is not).
bool past_near_plane(float depth) { return depth >= kNearPlane; }

bool is_finite(ScreenPoint point) { return std::isfinite(point.x) && std::isfinite(point.y); }

}  // namespace

Transform viewed_from(const Transform& camera_world, const Transform& offset) {
  Transform viewed = camera_world;
  viewed.position = camera_world.position + rotate(camera_world.rotation, offset.position);
  viewed.rotation = normalize(camera_world.rotation * offset.rotation);
  return viewed;
}

bool has_area(const ScreenFrame& frame) { return frame.screen_width > 0 && frame.screen_height > 0; }

CameraRay screen_ray(const Transform& camera_world, const Lens& lens, const ScreenFrame& frame, ScreenPoint point) {
  const PictureBox box = picture_box(frame);
  const Spread spread = spread_of(lens, box.aspect);
  const Centered at = centered(box.shown, point);
  const Vec3 aside =
      camera_world.right() * (at.across * spread.across) + camera_world.up() * (at.upward * spread.upward);
  // An orthographic lens sends parallel rays from across its picture; a perspective one fans them
  // out from where the camera is.
  if (is_orthographic(lens)) return {camera_world.position + aside, camera_world.forward()};
  return {camera_world.position, normalize(camera_world.forward() + aside)};
}

std::optional<ScreenPoint> world_to_screen(const Transform& camera_world, const Lens& lens, const ScreenFrame& frame,
                                           Vec3 point) {
  if (!has_area(frame)) return std::nullopt;
  const Vec3 offset = point - camera_world.position;
  const float depth = dot(offset, camera_world.forward());
  if (!past_near_plane(depth)) return std::nullopt;
  const PictureBox box = picture_box(frame);
  const Spread spread = spread_of(lens, box.aspect);
  const float spreading = is_orthographic(lens) ? 1.0f : depth;  // (a perspective picture widens with depth)
  const Centered at{dot(offset, camera_world.right()) / (spread.across * spreading),
                    dot(offset, camera_world.up()) / (spread.upward * spreading)};
  const ScreenPoint shown = on_screen(box.shown, at);
  if (!is_finite(shown)) return std::nullopt;
  return shown;
}

}  // namespace redgem
