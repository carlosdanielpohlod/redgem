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
inline constexpr const char* kFontsDirName = "fonts";  // game-provided TrueType/OpenType faces
inline constexpr const char* kScenesDirName = "scenes";
inline constexpr const char* kModelsDirName = "models";
inline constexpr const char* kLocalesDirName = "locales";  // the game's translations: <language>.json (I18n)
inline constexpr const char* kSceneExtension = ".glb";
inline constexpr const char* kMainSceneName = "main";  // the game's scene.glb
// scripts/<scene>_scene.rb sets a scene up from Ruby when it loads (see ruby/engine/scene_script.rb).
inline constexpr const char* kSceneScriptSuffix = "_scene";
inline constexpr const char* kRubyFileExtension = ".rb";
// What the editor names a file while it writes it, before the file takes its own name: a scene it
// saves, and a file it writes for a coding agent. A file named so is never part of a game.
inline constexpr const char* kSceneWritingSuffix = ".tmp";
inline constexpr const char* kAgentWritingSuffix = ".redgem-writing";

}  // namespace redgem::config
