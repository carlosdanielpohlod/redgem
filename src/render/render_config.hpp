#pragma once

// The renderer's numbers: what a scene is drawn with when it does not say, and the limits of what
// is drawn.

#include <cstddef>

namespace redgem::config {

inline constexpr int kTargetFps = 60;

// --- Cameras and lights ----------------------------------------------------------
inline constexpr float kDefaultCameraDistance = 10.0f;
inline constexpr float kDefaultSunIntensity = 1.0f;
// Used when the scene has no sun light (direction the light travels, normalized at runtime).
inline constexpr float kDefaultSunDirectionX = -1.0f;
inline constexpr float kDefaultSunDirectionY = -2.0f;
inline constexpr float kDefaultSunDirectionZ = -1.0f;
// Blender light strengths are exported raw: sun in W/m^2, point lights in Watts.
inline constexpr float kSunStrengthScale = 0.4f;
inline constexpr float kPointWattScale = 0.002f;
inline constexpr float kDefaultPointLightRange = 20.0f;
inline constexpr std::size_t kMaxVerticesPerMesh = 65535;  // raylib uses 16-bit indices
inline constexpr int kFpsCounterX = 10;
inline constexpr int kFpsCounterY = 10;

}  // namespace redgem::config
