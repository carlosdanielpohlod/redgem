#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "app/android/android_settings.hpp"
#include "app/prefab_budget.hpp"
#include "core/config.hpp"
#include "net/movement_rules.hpp"
#include "net/secure/server_pins.hpp"
#include "navigation/nav_settings.hpp"
#include "physics/body_limit.hpp"
#include "physics/collision_layers.hpp"
#include "physics/physics_config.hpp"
#include "render/display_settings.hpp"
#include "render/ui_scale.hpp"
#include "script/script_config.hpp"

// game.json: what a game says about itself. This is the one place that reads the file.
namespace redgem {

inline constexpr const char* kGameSettingsFile = "game.json";

struct GameSettings {
  std::string title;
  // Who makes the game: with the title, it names the folder the game's players keep their saves
  // and settings in (see app/game_data_location.hpp). Empty for a game that does not say.
  //   "studio": "My Studio"
  std::string studio;
  int window_width = 0;
  int window_height = 0;
  // What the game is known by wherever it is kept and whatever it is called: its Android app is
  // named after it unless the app is given a package. Empty for a game that has none.
  //   "id": "com.mystudio.mygame"   or   "id": "my-game-3fa2c1b0"
  std::string id;
  AndroidSettings android;
  // How the game is shown until its player chooses otherwise: the window's size above, and
  //   "fullscreen": true, "vsync": false, "fps_cap": 144 (0: none), "render_scale": 0.75,
  //   "quality": "medium"
  DisplaySettings display;
  // The screen the game's interface (UI) is laid out for, and which of its sides follows the
  // screen's: the "ui" section. Without one, the game's own window, its height matched:
  //   "ui": {"reference": [1920, 1080], "match": "height"}     ("width", "height" or "fit")
  UiReference ui;
  // How long one call into the game's scripts may run before it is stopped (see
  // script/script_budget.hpp), in milliseconds, 1 to config::kMaxScriptBudgetMs:
  //   "script_budget_ms": 2000
  int script_budget_ms = config::kDefaultScriptBudgetMs;
  // How many fixed steps the game takes a second (physics, fixed_update, Time.fixed_delta, a
  // tool's or a test's frame), config::kMinTickHz to config::kMaxTickHz:
  //   "tick_hz": 128
  int tick_hz = config::kDefaultTickHz;
  // How many poses a second NetworkSync sends of an object that says nothing itself:
  //   "network": {"send_hz": 64}       (a "network" object also says the game plays online)
  int send_hz = config::kDefaultSendHz;
  // How far back, in milliseconds, the host rewinds to judge a shot (Physics.rewind):
  //   "network": {"max_rewind_ms": 150}
  int max_rewind_ms = config::kDefaultMaxRewindMs;
  // Which bodies collide with which: the "physics" section (see physics/collision_layers.hpp).
  CollisionLayers collision_layers;
  // How many threads solve the game's physics: the "threads" key of the "physics" section (see
  // physics/physics_threads.hpp). 1 solves it as RedGem always did.
  int physics_threads = config::kDefaultPhysicsThreads;
  // How many bodies a scene's physics world holds at once (see physics/body_limit.hpp):
  //   "physics": {"max_bodies": 16384}
  std::uint32_t max_bodies = kDefaultMaxBodies;
  // The relays and dedicated servers the game knows by their fingerprints: the "servers" section
  // (see net/secure/server_pins.hpp). Joining one of them checks that it is the one.
  std::vector<net::ServerPin> servers;
  // How big the characters that walk the level are, and how finely it is baked: the "navigation"
  // section (see navigation/nav_settings.hpp).
  NavSettings navigation;
  // What the host holds the others' moves to, an anti-cheat: the "movement_validation" section
  // (see net/movement_rules.hpp). A game without it checks nothing.
  net::MovementRules movement;
  // How the game's menus look (GameMenu, UI::Theme): the "menu" section, as its JSON text; empty
  // for a game that has none.
  //   "menu": {"theme": {"colors": {"button": "#203040"}}, "screens": {"title": {...}}}
  std::string menu;
  // How big a scene's prefab files may be (see app/prefab_budget.hpp):
  //   "prefab_budget_mb": {"source": 1024, "decoded": 2048}
  PrefabBudget prefab_budget;
};

// Everything game.json says, read once.
struct GameManifest {
  // Always usable: the defaults (folder name as title, the engine's window size, an app with
  // nothing of its own) stand in for whatever is missing or is not what it should be.
  GameSettings settings;
  bool file_parsed = false;
  // Whether game.json gives the title (the folder's name stands in for one it does not give).
  bool title_given = false;
  // What is wrong with the "android" section, for whoever packs the app; empty when it can be
  // used. (A game plays all the same: the section only matters to a build for Android.)
  std::string android_error;
  // What is wrong with the "id"; empty when it can be used or the game has none. (A game plays
  // all the same, as one with no id does.)
  std::string id_error;
  // What is wrong with "script_budget_ms"; empty when it can be used or the file has none. (The
  // scripts get the engine's default instead.)
  std::string script_budget_error;
  // What is wrong with "tick_hz"; empty when it can be used or the file has none. (The game ticks
  // at config::kDefaultTickHz instead; `redgem check` refuses it.)
  std::string tick_hz_error;
  // What is wrong with the "network" section's "send_hz"; empty when it can be used or there is
  // none. (Objects send config::kDefaultSendHz poses a second instead; `redgem check` refuses it.)
  std::string send_hz_error;
  // The same for its "max_rewind_ms". (The host rewinds config::kDefaultMaxRewindMs at most instead;
  // `redgem check` refuses it.)
  std::string max_rewind_error;
  // What is wrong with "prefab_budget_mb"; empty when it can be used or the file has none. (The
  // prefabs get the engine's budgets instead; `redgem check` refuses it.)
  std::string prefab_budget_error;
  // What is wrong with the "physics" section; each problem is left out, the rest is used.
  std::vector<std::string> physics_warnings;
  // What is wrong with the "physics" section's "max_bodies"; empty when it can be used or the
  // section has none. (The world holds kDefaultMaxBodies instead; `redgem check` refuses it.)
  std::string max_bodies_error;
  // What is wrong with the "servers" section; each entry that cannot be used is left out.
  std::vector<std::string> servers_warnings;
  // What is wrong with the "navigation" section; each such setting keeps its default.
  std::vector<std::string> navigation_warnings;
  // What is wrong with the "movement_validation" section; each such setting keeps its default.
  std::vector<std::string> movement_warnings;
  // What is wrong with the "ui" section; each such setting keeps its default.
  std::vector<std::string> ui_warnings;
};
GameManifest read_game_manifest(const std::filesystem::path& game_dir);
// What is wrong with the "physics" section, a line each (its layers', then its body limit's).
std::vector<std::string> physics_problems(const GameManifest& manifest);

// Whether a text can be a game's id: small letters, digits, '.', '-' and '_', starting and ending
// with a letter or a digit, with no two dots in a row, up to kMaxGameIdLength long.
inline constexpr std::size_t kMaxGameIdLength = 100;
bool valid_game_id(const std::string& id);
// The settings alone, for whoever has no use for what is wrong with the file.
GameSettings read_game_settings(const std::filesystem::path& game_dir);
// The limits the game's scenes expand their prefabs within (its "prefab_budget_mb").
prefabs::Limits read_prefab_limits(const std::filesystem::path& game_dir);
// Writes the title, the window's size, the studio and the id (when the settings have them), and the
// physics' threads (taken out of the file for the default). What else the file holds (its "android"
// section, an id the settings have none for...) stays as it is.
bool write_game_settings(const std::filesystem::path& game_dir, const GameSettings& settings, std::string& error);

}  // namespace redgem
