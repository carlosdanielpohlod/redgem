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

// The pictures a test takes of the game it plays (t.screenshot). A test asks by name; the picture
// is of the next frame drawn, written by whoever shows the game once that frame is drawn.
class FramePictures {
 public:
  explicit FramePictures(std::filesystem::path folder) : folder_(std::move(folder)) {}

  // Asks for a picture of the next frame drawn.
  void ask(const std::string& name);
  // The files still to be written.
  const std::vector<std::filesystem::path>& pending() const { return pending_; }
  // Writes what was drawn in the window to every file asked for. To be called once a frame is
  // drawn and before it is shown.
  void save_pending();
  // Drops what was asked for and not drawn (a game nobody shows has no pictures).
  void forget_pending() { pending_.clear(); }

 private:
  std::filesystem::path folder_;
  std::vector<std::filesystem::path> pending_;
};

// Defines TestScreen in a Ruby VM: TestScreen.capture(name) asks `pictures` for a picture of the
// next frame. The engine knows nothing of it, so the VM itself remembers which pictures those
// are. `pictures` has to outlive the VM.
void install_test_screen(mrb_state* mrb, FramePictures& pictures);

}  // namespace redgem::testing
