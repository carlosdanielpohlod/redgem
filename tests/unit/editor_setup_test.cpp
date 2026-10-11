#include <doctest/doctest.h>

#include <algorithm>
#include <set>
#include <string>

#include "project/editor_setup.hpp"
#include "project/game_build.hpp"

using namespace redgem;

TEST_CASE("the editor is opened able to build for every system a game can be built for") {
  const std::vector<BuildChoice> choices = build_choices();
  std::set<std::string> names;
  for (const BuildChoice& choice : choices) {
    CAPTURE(choice.name);
    CHECK(build_target_from_name(choice.name).has_value());
    CHECK_FALSE(choice.label.empty());
    CHECK(static_cast<bool>(choice.build));
    names.insert(choice.name);
  }
  CHECK(names == std::set<std::string>{"android", "linux", "windows"});
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
  CHECK_FALSE(own->build("/nowhere/no-such-game", "/nowhere/out", error).has_value());
  CHECK(error.find("is not a game") != std::string::npos);
}

TEST_CASE("the program's editor gets the card it was told and its builds; a bare one builds nothing") {
  const EditorOptions options = editor_options(GpuChoice::Integrated);
  CHECK(options.gpu == GpuChoice::Integrated);
  CHECK(options.builds.size() == build_choices().size());
  CHECK(EditorOptions{}.builds.empty());
}
