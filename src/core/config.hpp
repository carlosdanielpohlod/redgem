#pragma once

// What every part of the engine agrees on: how time is stepped and how a game's folder is laid
// out. Each layer keeps its own numbers next to its code (physics/physics_config.hpp,
// render/render_config.hpp, ...); the properties scenes carry are in scene/props.hpp.

namespace redgem::config {

// --- Time ---------------------------------------------------------------------
// How many fixed steps (physics, fixed_update) a game takes a second: game.json's "tick_hz", from
// kMinTickHz to kMaxTickHz. A game that says nothing ticks at kDefaultTickHz.
inline constexpr int kDefaultTickHz = 60;
inline constexpr int kMinTickHz = 30;
inline constexpr int kMaxTickHz = 128;
// The seconds of one step at that many steps a second.
inline constexpr float tick_seconds(int tick_hz) { return 1.0f / static_cast<float>(tick_hz); }
// A step of a game that says nothing: for whoever has no game (a bare physics world, a test).
inline constexpr float kDefaultFixedDeltaSeconds = tick_seconds(kDefaultTickHz);
// How many poses a second NetworkSync sends of an object: game.json's "network": {"send_hz": N},
// from kMinSendHz to kMaxSendHz (as often as the fastest tick at most); kDefaultSendHz when it
// says nothing. An object's own send_hz setting takes the same numbers.
inline constexpr int kDefaultSendHz = 20;
inline constexpr int kMinSendHz = 1;
inline constexpr int kMaxSendHz = kMaxTickHz;
// Upper bound for a single rendered frame so a debugger pause does not explode the simulation.
inline constexpr float kMaxFrameSeconds = 0.25f;
inline constexpr int kMaxFixedStepsPerFrame = 8;  // at kDefaultTickHz
// How many steps a frame takes at most, at that many steps a second: as much game time as
// kMaxFixedStepsPerFrame steps of a game that says nothing (rounded up), so a game that ticks
// faster catches up on as long a hitch.
inline constexpr int max_steps_per_frame(int tick_hz) {
  return (kMaxFixedStepsPerFrame * tick_hz + kDefaultTickHz - 1) / kDefaultTickHz;
}

// --- Game folder layout ---------------------------------------------------------
inline constexpr const char* kSceneFileName = "scene.glb";
inline constexpr const char* kImagesDirName = "images";  // pictures drawn with UI.image
inline constexpr const char* kScriptsDirName = "scripts";
inline constexpr const char* kSoundsDirName = "sounds";
inline constexpr const char* kFontsDirName = "fonts";  // game-provided TrueType/OpenType faces
inline constexpr const char* kScenesDirName = "scenes";
inline constexpr const char* kModelsDirName = "models";
inline constexpr const char* kPrefabsDirName = "prefabs";      // reusable linked GLB hierarchies
inline constexpr const char* kLocalesDirName = "locales";      // the game's translations: <language>.json (I18n)
inline constexpr const char* kDialoguesDirName = "dialogues";  // its conversations: <name>.json (Dialogue)
inline constexpr const char* kQuestsDirName = "quests";        // its quests: <name>.json (Quests)
inline constexpr const char* kItemsDirName = "items";          // the items its quests name: <name>.json
// The ground layers of its terrains: <name>.json, with their weights beside it (<name>.splat0.png).
inline constexpr const char* kTerrainsDirName = "terrains";
inline constexpr const char* kSceneExtension = ".glb";
inline constexpr const char* kMainSceneName = "main";  // the game's scene.glb
// scripts/<scene>_scene.rb sets a scene up from Ruby when it loads (see ruby/engine/scene_script.rb).
inline constexpr const char* kSceneScriptSuffix = "_scene";
inline constexpr const char* kRubyFileExtension = ".rb";
// What the editor names a file while it writes it, before the file takes its own name: a scene it
// saves, and a file it writes for a coding agent. A file named so is never part of a game.
inline constexpr const char* kSceneWritingSuffix = ".tmp";
inline constexpr const char* kAgentWritingSuffix = ".redgem-writing";
// Farthest a point the tools are given (a query's, an object's placement) may be from the origin
// on any axis, in meters, and the largest number a placement takes: well past any level, and near
// enough that sums and rays of such numbers stay finite in single precision.
inline constexpr float kLargestCoordinate = 1.0e6f;

}  // namespace redgem::config
