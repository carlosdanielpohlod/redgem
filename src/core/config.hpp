#pragma once

// What every part of the engine agrees on: how time is stepped and how a game's folder is laid
// out. Each layer keeps its own numbers next to its code (physics/physics_config.hpp,
// render/render_config.hpp, ...); the properties scenes carry are in scene/props.hpp.

namespace redgem::config {

// --- Time ---------------------------------------------------------------------
inline constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
// Upper bound for a single rendered frame so a debugger pause does not explode the simulation.
inline constexpr float kMaxFrameSeconds = 0.25f;
inline constexpr int kMaxFixedStepsPerFrame = 8;

// --- Game folder layout ---------------------------------------------------------
inline constexpr const char* kSceneFileName = "scene.glb";
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

}  // namespace redgem::config
