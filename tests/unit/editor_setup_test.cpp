#include <doctest/doctest.h>

#include <algorithm>
#include <set>
#include <string>

#include "editor/editor_app.hpp"
#include "project/editor_setup.hpp"
#include "project/game_build.hpp"

using namespace redgem;

namespace {

constexpr float kViewWidth = 1280.0f;
constexpr float kViewHeight = 720.0f;

}  // namespace

TEST_CASE("a game played in the editor has no touch screen: no controls are laid over it, whatever it asks") {
  GatedInputSource input;
  input.enabled = true;
  input.show_touch_controls(true);
  input.fit_surface(kViewWidth, kViewHeight);
  CHECK(input.touch_layout() == nullptr);
}

TEST_CASE("the editor is opened able to build for every system it can build a game for") {
  const std::vector<BuildChoice> choices = build_choices();
  std::set<std::string> names;
  for (const BuildChoice& choice : choices) {
    CAPTURE(choice.name);
    CHECK(build_target_from_name(choice.name).has_value());
    CHECK_FALSE(choice.label.empty());
    CHECK(static_cast<bool>(choice.build));
    names.insert(choice.name);
  }
  // (A game for the Mac is only built on a Mac.)
  const std::set<std::string> offered{"android", "linux", "windows", build_target_name(host_build_target())};
  CHECK(names == offered);
  CHECK(names.size() == choices.size());
}

TEST_CASE("exactly one of the editor's builds is for the system it runs on") {
  const std::vector<BuildChoice> choices = build_choices();
  const auto own = [](const BuildChoice& choice) { return choice.for_this_system; };
  REQUIRE(std::count_if(choices.begin(), choices.end(), own) == 1);
  CHECK(std::find_if(choices.begin(), choices.end(), own)->name == build_target_name(host_build_target()));
}

TEST_CASE("a build the editor offers says why it could not be made") {
  const std::vector<BuildChoice> choices = build_choices();
  const auto own = std::find_if(choices.begin(), choices.end(), [](const BuildChoice& c) { return c.for_this_system; });
  REQUIRE(own != choices.end());
  std::string error;
  CHECK_FALSE(own->build("/nowhere/no-such-game", "/nowhere/out", {}, error).has_value());
  CHECK(error.find("is not a game") != std::string::npos);
}

TEST_CASE("the program's editor gets the card it was told and its builds; a bare one builds nothing") {
  const EditorOptions options = editor_options(GpuChoice::Integrated);
  CHECK(options.gpu == GpuChoice::Integrated);
  CHECK(options.builds.size() == build_choices().size());
  CHECK(EditorOptions{}.builds.empty());
}
