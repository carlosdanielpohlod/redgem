#pragma once

#include <mruby.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace redgem::testing {

// The Ruby module a test asks for pictures of the game through.
inline constexpr const char* kTestScreenModule = "TestScreen";

// The file a picture a test named is written to: `folder`/<name>.png. Nothing, after a warning,
// for a name that would put the file anywhere else.
std::optional<std::filesystem::path> picture_file(const std::filesystem::path& folder, const std::string& name);

// How far a picture is from a baseline: the share of its pixels (0 to 1) with a channel further
// than kBaselineTolerance from the baseline's; 1 for pictures of other sizes; nullopt when either
// file cannot be read.
inline constexpr int kBaselineTolerance = 24;  // what drawing on another graphics card may move a channel
std::optional<float> picture_difference(const std::filesystem::path& picture, const std::filesystem::path& baseline);

// The environment variable that, set, makes TestScreen.difference write each picture as its own
// baseline (one of its name) instead, and say it is none away from it: to record the baselines
// again on purpose. A picture compared with another's baseline is still compared.
inline constexpr const char* kUpdateBaselinesVariable = "REDGEM_UPDATE_BASELINES";

// The pictures a test takes of the game it plays (t.screenshot). A test asks by name; the picture
// is of the next frame drawn, written by whoever shows the game once that frame is drawn.
class FramePictures {
 public:
  // `game_dir`: where the baselines the pictures are compared with are (TestScreen.difference).
  explicit FramePictures(std::filesystem::path folder, std::filesystem::path game_dir = {})
      : folder_(std::move(folder)), game_dir_(std::move(game_dir)) {}

  // Asks for a picture of the next frame drawn.
  void ask(const std::string& name);
  // The files still to be written.
  const std::vector<std::filesystem::path>& pending() const { return pending_; }
  // Writes what was drawn in the window to every file asked for. To be called once a frame is
  // drawn and before it is shown.
  void save_pending();
  // The file a picture taken was written to, and the game's file a baseline is in ("" when the
  // name would put either anywhere else).
  std::filesystem::path picture_path(const std::string& name) const;
  std::filesystem::path baseline_path(const std::string& relative) const;
  // Drops what was asked for and not drawn (a game nobody shows has no pictures).
  void forget_pending() { pending_.clear(); }

 private:
  std::filesystem::path folder_;
  std::filesystem::path game_dir_;
  std::vector<std::filesystem::path> pending_;
};

// Defines TestScreen in a Ruby VM: TestScreen.capture(name) asks `pictures` for a picture of the
// next frame; TestScreen.difference(name, baseline) says how far the picture taken under that name
// is from a baseline picture of the game's folder (picture_difference). The engine knows nothing
// of it, so the VM itself remembers which pictures those are. `pictures` has to outlive the VM.
void install_test_screen(mrb_state* mrb, FramePictures& pictures);

}  // namespace redgem::testing
