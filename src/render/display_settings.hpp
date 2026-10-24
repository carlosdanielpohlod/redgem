#pragma once

// What the player of a game chooses about how it is shown: the window (its mode, its size, how
// sharp its picture is, the pace of its frames) and how much the renderer does (a quality preset).
// A game's game.json gives the defaults; the player's choices are kept in the user's settings.

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "render/quality.hpp"
#include "render/render_config.hpp"

namespace redgem {

// Width and height of something drawn on, in pixels.
struct SurfaceSize {
  int width = 0;
  int height = 0;
};

inline bool operator==(SurfaceSize a, SurfaceSize b) { return a.width == b.width && a.height == b.height; }
inline bool operator!=(SurfaceSize a, SurfaceSize b) { return !(a == b); }

enum class WindowMode {
  Windowed,
  Fullscreen,  // the monitor is switched to the window's size, and the window covers it
  Borderless,  // a window without borders as big as the monitor, at the monitor's own size
};

// Render scale: the share of the window's size the game's world is drawn at before it is
// stretched over the window (the interface is always drawn at the window's own size).
inline constexpr float kSmallestRenderScale = 0.5f;
inline constexpr float kLargestRenderScale = 2.0f;
inline constexpr float kFullRenderScale = 1.0f;  // drawn at the window's size: nothing is stretched
inline constexpr int kNoFrameCap = 0;            // frames come as fast as they are drawn
inline constexpr int kMostFramesPerSecond = 1000;
inline constexpr float kPlayersFullVolume = 1.0f;

struct DisplaySettings {
  WindowMode mode = WindowMode::Windowed;
  // The window's size (and the monitor's, in fullscreen). The game's window_width and
  // window_height unless the player chose another.
  SurfaceSize resolution;
  float render_scale = kFullRenderScale;
  bool vsync = true;                 // frames wait for the monitor's refresh
  int fps_cap = config::kTargetFps;  // frames a second at most; kNoFrameCap for no limit
  QualityPreset quality = QualityPreset::Ultra;
  QualityCaps custom_quality;         // what QualityPreset::Custom allows
  float volume = kPlayersFullVolume;  // the game's master volume, 0 (silent) to 1
};

const char* window_mode_name(WindowMode mode);  // "windowed", "fullscreen", "borderless"
std::optional<WindowMode> window_mode_from_name(const std::string& name);
// What the player's quality allows: its preset's caps, or custom_quality for QualityPreset::Custom.
QualityCaps quality_caps(const DisplaySettings& display);

// A render scale held within kSmallestRenderScale..kLargestRenderScale (kFullRenderScale when it
// is not a number).
float held_render_scale(float scale);
// Whether the world is drawn at another size than the window's.
bool scales_picture(float render_scale);
// The size the world is drawn at in a window of `window`'s size: never less than a pixel a side.
SurfaceSize scaled_size(SurfaceSize window, float render_scale);
// A frame cap held to what a cap can be: kNoFrameCap or 1..kMostFramesPerSecond.
int held_fps_cap(int cap);

// The settings as the user's settings file keeps them, and as scripts see them (Settings):
//   {"window_mode": "windowed", "width": 1280, "height": 720, "render_scale": 1.0, "vsync": true,
//    "fps_cap": 60, "quality": "ultra", "volume": 1.0,
//    "custom_quality": {"msaa": 8, "antialias": true, "shadow_cascades": 4, "shadow_distance": 1.0,
//                       "scatter_density": 1.0, "scatter_distance": 1.0, "sky_detail": 1.0,
//                       "ambient_occlusion": 1.0, "bloom": 1.0}}
nlohmann::json display_json(const DisplaySettings& display);
// `base` with what `json` says in place of its own: whatever is missing, or is not what it should
// be, stays as `base` has it; what is out of range is held to the range.
DisplaySettings display_from_json(const nlohmann::json& json, DisplaySettings base);

// Sizes a window can take on a monitor of `monitor`'s size: the usual ones that fit, smallest
// first, and the monitor's own.
std::vector<SurfaceSize> window_sizes_for(SurfaceSize monitor);

}  // namespace redgem
