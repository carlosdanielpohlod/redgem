#pragma once

// The numbers of a scene's data: how its meshes are laid out in memory and what its objects
// default to. Whoever reads or writes that data (loader, physics, renderer, editor) shares them.

namespace redgem::config {

// --- Cameras --------------------------------------------------------------------
inline constexpr float kDefaultFovYDegrees = 45.0f;

// --- Meshes ---------------------------------------------------------------------
// Skinning: joints and weights per vertex (glTF), and the most joints one skeleton may have (what
// is drawn with may hold fewer: see render/gpu_profile.hpp).
inline constexpr int kJointsPerVertex = 4;
inline constexpr int kMaxSkinJoints = 64;
inline constexpr int kFloatsPerPosition = 3;
inline constexpr int kFloatsPerNormal = 3;
inline constexpr int kFloatsPerTexcoord = 2;
inline constexpr int kChannelsPerColor = 4;
inline constexpr int kVerticesPerTriangle = 3;
inline constexpr unsigned char kMaxColorByte = 255;

}  // namespace redgem::config
