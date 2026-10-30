#pragma once

#include <optional>

#include "scene/transform.hpp"

namespace redgem {

// Nearer than this (meters in front of the camera) the renderer draws nothing: raylib's near clip
// plane, which every game camera is drawn with.
inline constexpr float kNearPlane = 0.01f;

// What a camera sees the world through.
struct Lens {
  // Perspective: the field of view from top to bottom; the engine's default when not positive.
  float fov_y_radians = 0.0f;
  // Orthographic when positive: how many meters the picture spans from top to bottom.
  float ortho_height = 0.0f;
};

// The screen the interface is drawn on (UI.width by UI.height), and the world's picture shown on
// it: drawn at its own size (the render scale's share of the window), then stretched over the
// screen as large as fits, in its middle. A picture of no size is the screen's own size.
struct ScreenFrame {
  int screen_width = 0;
  int screen_height = 0;
  int picture_width = 0;
  int picture_height = 0;
};

// A point of the screen, in pixels from its top left corner.
struct ScreenPoint {
  float x = 0.0f;
  float y = 0.0f;
};

// A ray from where it starts, the way it goes (normalized).
struct CameraRay {
  Vec3 origin;
  Vec3 direction;
};

// Where a camera sees from: its place in the world, moved and turned by `offset` in its own axes
// (its shake, see CameraDesc).
Transform viewed_from(const Transform& camera_world, const Transform& offset);

// Whether the screen has a size: there is nothing to see through otherwise.
bool has_area(const ScreenFrame& frame);

// The ray that leaves a camera through a point of the screen. The screen must have an area.
CameraRay screen_ray(const Transform& camera_world, const Lens& lens, const ScreenFrame& frame, ScreenPoint point);

// Where a point of the world is seen on the screen, the inverse of screen_ray: nothing for a point
// behind the camera or nearer than its near plane, or for a screen of no area. A point in front
// of the camera but outside its view gets a place off the screen (left of it, below it...).
std::optional<ScreenPoint> world_to_screen(const Transform& camera_world, const Lens& lens, const ScreenFrame& frame,
                                           Vec3 point);

}  // namespace redgem
