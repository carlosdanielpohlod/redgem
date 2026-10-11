#pragma once

// Every tunable number of the engine lives here, named. No magic numbers elsewhere.

#include <cstddef>
#include <cstdint>

namespace redgem::config {

// --- Window -------------------------------------------------------------------
inline constexpr int kWindowWidth = 1280;
inline constexpr int kWindowHeight = 720;
inline constexpr const char* kWindowTitle = "RedGem";
// Width of a letter as a share of the text size, to estimate text widths when there is no font
// to measure with (headless runs).
inline constexpr float kEstimatedLetterWidth = 0.55f;
inline constexpr int kTargetFps = 60;

// --- Time ---------------------------------------------------------------------
inline constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
// Upper bound for a single rendered frame so a debugger pause does not explode the simulation.
inline constexpr float kMaxFrameSeconds = 0.25f;
inline constexpr int kMaxFixedStepsPerFrame = 8;

// --- Game folder layout ---------------------------------------------------------
inline constexpr const char* kSceneFileName = "scene.glb";
// The running program's own file (Linux): what Build copies next to a game.
inline constexpr const char* kSelfExecutable = "/proc/self/exe";
inline constexpr const char* kImagesDirName = "images";  // pictures drawn with UI.image
inline constexpr const char* kScriptsDirName = "scripts";
inline constexpr const char* kSoundsDirName = "sounds";
inline constexpr const char* kScenesDirName = "scenes";
inline constexpr const char* kModelsDirName = "models";
inline constexpr const char* kSceneExtension = ".glb";
inline constexpr const char* kMainSceneName = "main";  // the game's scene.glb
// scripts/<scene>_scene.rb sets a scene up from Ruby when it loads (see ruby/engine/scene_script.rb).
inline constexpr const char* kSceneScriptSuffix = "_scene";
inline constexpr const char* kRubyFileExtension = ".rb";
inline constexpr const char* kDefaultScreenshotDir = "screenshots";
inline constexpr const char* kScreenshotExtension = ".png";

// --- Tests ------------------------------------------------------------------------
// A system test that has not finished after this many frames fails.
inline constexpr long long kMaxTestFrames = 3600;

// --- Colliders without a mesh (e.g. Blender empties) -------------------------------
inline constexpr float kDefaultColliderHalfExtent = 0.5f;

// --- Blender custom properties (glTF node extras) -------------------------------
inline constexpr const char* kPropScript = "script";
inline constexpr const char* kPropBody = "body";
inline constexpr const char* kPropCollider = "collider";
inline constexpr const char* kPropTrigger = "trigger";
inline constexpr const char* kPropTag = "tag";
inline constexpr const char* kPropMass = "mass";
inline constexpr const char* kPropVisible = "visible";
inline constexpr const char* kPropWheel = "wheel";
// A terrain: a square grid mesh made in the editor, whose ground can be sculpted there.
inline constexpr const char* kPropTerrain = "terrain";
inline constexpr const char* kPropTerrainSize = "terrain_size";    // meters across
inline constexpr const char* kPropTerrainCells = "terrain_cells";  // quads per side
inline constexpr const char* kPropPrefab = "prefab";
inline constexpr const char* kPropTint = "tint";
// Levels of detail: an object is drawn only while its distance from the camera (meters) is at
// least `visible_from` and less than `visible_to`. Several versions of a model, each with its own
// range, make a detailed one up close and a light one far away.
inline constexpr const char* kPropVisibleFrom = "visible_from";
inline constexpr const char* kPropVisibleTo = "visible_to";

// --- Physics --------------------------------------------------------------------
inline constexpr float kGravityY = -20.0f;
inline constexpr float kDefaultBodyMass = 1.0f;
inline constexpr std::uint32_t kMaxBodies = 4096;
inline constexpr std::uint32_t kNumBodyMutexes = 0;  // 0 = Jolt picks a default
inline constexpr std::uint32_t kMaxBodyPairs = 4096;
inline constexpr std::uint32_t kMaxContactConstraints = 4096;
inline constexpr std::size_t kPhysicsTempAllocatorBytes = 10u * 1024u * 1024u;
inline constexpr std::uint32_t kMaxPhysicsJobs = 1024;
inline constexpr int kPhysicsCollisionSteps = 1;
// Smallest half extent accepted for a collider (degenerate meshes such as planes).
inline constexpr float kMinColliderHalfExtent = 0.01f;
// Convex radius is clamped to this fraction of the smallest half extent.
inline constexpr float kConvexRadiusFraction = 0.5f;

// --- Character controller -------------------------------------------------------
inline constexpr float kCharacterMaxSlopeDegrees = 50.0f;
inline constexpr float kCharacterMass = 70.0f;
inline constexpr float kCharacterMaxStrength = 100.0f;
inline constexpr float kCharacterPadding = 0.02f;
inline constexpr float kCharacterPenetrationRecoverySpeed = 1.0f;
inline constexpr float kCharacterPredictiveContactDistance = 0.1f;
inline constexpr float kCharacterStickToFloorDistance = 0.5f;
inline constexpr float kCharacterStairStepHeight = 0.4f;
// Shortest cylinder section allowed in a capsule built from flat or very round meshes.
inline constexpr float kCharacterMinCylinderHalfHeight = 0.05f;

// --- Vehicles -------------------------------------------------------------------
inline constexpr float kVehicleDefaultMass = 1200.0f;
inline constexpr float kVehicleMaxSteerDegrees = 30.0f;
inline constexpr float kVehicleMaxPitchRollDegrees = 60.0f;
inline constexpr float kVehicleSuspensionMinLength = 0.05f;
inline constexpr float kVehicleSuspensionMaxLength = 0.35f;
inline constexpr float kVehicleHandBrakeTorque = 4000.0f;
inline constexpr float kVehicleEngineMaxTorque = 600.0f;
// Lowers the chassis center of mass by this fraction of its half height (keeps cars upright).
inline constexpr float kVehicleCenterOfMassDrop = 0.6f;
inline constexpr float kVehicleWheelConvexRadiusFraction = 0.1f;

// --- Rendering ------------------------------------------------------------------
inline constexpr float kDefaultFovYDegrees = 45.0f;
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
inline constexpr int kDefaultTextSize = 24;
inline constexpr int kFpsCounterX = 10;
inline constexpr int kFpsCounterY = 10;

// --- Rendering of meshes ----------------------------------------------------------
// Skinning: joints and weights per vertex (glTF), and how many joints one skeleton may have
// (the size of the shader's bone array).
inline constexpr int kJointsPerVertex = 4;
inline constexpr int kMaxSkinJoints = 64;
inline constexpr int kFloatsPerPosition = 3;
inline constexpr int kFloatsPerNormal = 3;
inline constexpr int kFloatsPerTexcoord = 2;
inline constexpr int kChannelsPerColor = 4;
inline constexpr int kVerticesPerTriangle = 3;
inline constexpr unsigned char kMaxColorByte = 255;

}  // namespace redgem::config
