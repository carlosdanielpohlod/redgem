#pragma once

// What whoever opens the editor decides for it.

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "editor/agent/acp_messages.hpp"
#include "render/gpu_select.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

class EditorApp;

// How long the frame about to be drawn counts for, in seconds; asked once a frame.
using FrameClock = std::function<float()>;

// Hears each step a build begins ("copying the game's files", "signing the APK"), on the thread
// that builds.
using BuildProgress = std::function<void(const std::string& step)>;

// One way of making a standalone game, as Build Game offers it. The editor only knows that such
// ways exist: whoever opens it says which, and does the building.
struct BuildChoice {
  std::string name;   // the system built for ("linux"): how the build is spoken of, and its folder named
  std::string label;  // what the menu shows
  // Whether it is for the system the editor runs on (the build then needs no system in its folder's name).
  bool for_this_system = false;
  // Builds the game in `game_dir` into `out_dir`, telling `progress` (which may be empty) each step
  // it begins. The game's executable (or app), or nullopt with `error` saying why not. The editor
  // calls it on a thread of its own: it works only on the files it is given.
  std::function<std::optional<std::filesystem::path>(const std::filesystem::path& game_dir,
                                                     const std::filesystem::path& out_dir,
                                                     const BuildProgress& progress, std::string& error)>
      build;
};

// What the Agent panel is given by whoever opens the editor (see ui/agent_panel.hpp): the editor
// knows the tools it hands an agent only through these.
struct AgentHooks {
  std::string client_version;  // what the editor tells an agent it is
  // The MCP servers a new session gets for this editor (its live tools); empty, with `error`, when
  // there are none. Empty function: none.
  std::function<std::vector<agent::McpServer>(EditorApp& editor, std::string& error)> tool_servers;
  // An object of the scene as `redgem scene get` shows it, for the context a prompt carries.
  std::function<std::string(const SceneDesc& scene, int object)> describe_object;
  // Where the list of agents is kept; empty: agents.json in the user's data folder.
  std::filesystem::path agents_file;
  // Where each game's past conversations are kept (empty: none are), and whether the last one comes
  // back as the editor opens the game.
  std::filesystem::path sessions_file;
  bool reopen_last_session = false;
};

struct EditorOptions {
  // How long each frame counts for, for whoever drives the editor at a pace of its own. Empty:
  // the time the window measured between its last two frames.
  FrameClock clock;
  // The graphics card the window opens on, where the machine has two.
  GpuChoice gpu = GpuChoice::Auto;
  // The ways of building a game, in the order Build Game lists them. None: the editor builds nothing.
  std::vector<BuildChoice> builds;
  AgentHooks agent;
};

}  // namespace redgem
