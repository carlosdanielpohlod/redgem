#pragma once

// A game played by a tool (`redgem run`, `redgem shot`): the same run every time (each frame one
// fixed step, nothing pressed, no sound), on the scene asked for, saving into a folder of the
// run's own that goes with it, so that it never reads or changes a player's progress.

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "app/presenter.hpp"
#include "project/tool_line.hpp"

namespace redgem::tools {

// The options every tool that plays a game reads.
inline constexpr const char* kSceneOption = "--scene";
inline constexpr const char* kSecondsOption = "--seconds";
inline constexpr const char* kFramesOption = "--frames";

// What to play.
struct PlayRequest {
  std::filesystem::path game_dir;
  std::string scene;                  // "main" is scene.glb
  std::optional<std::string> camera;  // the object whose camera shows the game (else the first camera)
};

// A folder of one run's own, empty as it starts and gone with it.
class ScratchFolder {
 public:
  ScratchFolder();
  ~ScratchFolder();
  ScratchFolder(const ScratchFolder&) = delete;
  ScratchFolder& operator=(const ScratchFolder&) = delete;

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

// The game of `request` ready to play, shown by `presenter` (nobody when null), saving in
// `scratch`; why it cannot be played otherwise.
std::variant<std::unique_ptr<Engine>, std::string> engine_for(const PlayRequest& request, Presenter* presenter,
                                                              const ScratchFolder& scratch);

// Leaves the camera of the object `name` the only one of the scene, so it is the one that shows
// it. Why not, when no object of that name has a camera (the cameras there are, listed).
std::optional<std::string> keep_camera(SceneDesc& scene, const std::string& name);

// How many fixed steps of play `--seconds` or `--frames` ask for; `otherwise` seconds when they
// say nothing. Nothing when what they say is no length.
std::optional<long long> frames_asked(const ToolLine& line, double otherwise);

}  // namespace redgem::tools
