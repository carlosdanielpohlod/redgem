// The errors games run into most come with what to do about them.

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "core/exit_codes.hpp"
#include "core/files.hpp"
#include "project/error_hints.hpp"
#include "project/tool_commands.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::tools;
namespace fs = std::filesystem;

namespace {

const fs::path kPlatformer = REDGEM_PLATFORMER_DIR;

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("a method mistyped on one of the engine's classes is answered with the names near it") {
  CHECK(hint_for("undefined method 'positon=' for Transform (NoMethodError)", kPlatformer) ==
        "did you mean: position=, position (redgem api Transform)");
  CHECK(hint_for("undefined method 'zer' for Class (NoMethodError)", kPlatformer).find("Vector3.zero") !=
        std::string::npos);
  CHECK(hint_for("undefined method 'zzzzzzzz' for Transform (NoMethodError)", kPlatformer) ==
        "redgem api Transform lists its methods");
  // (The game's own classes are the game's: the reference knows nothing of their methods.)
  CHECK(hint_for("undefined method 'jump' for Coin (NoMethodError)", kPlatformer).empty());
}

TEST_CASE("a class no script defines is answered with the file it is looked for in") {
  CHECK(hint_for("uninitialized constant Spiner (NameError)", kPlatformer) ==
        "a class Spiner is looked for in scripts/spiner.rb (class Spiner < Behaviour)");
  CHECK(hint_for("uninitialized constant Playr (NameError)", kPlatformer) ==
        "a class Playr is looked for in scripts/playr.rb (class Playr < Behaviour) did you mean: Player");
}

TEST_CASE("a scene or a script the game lacks is answered with those it has") {
  CHECK(hint_for("no scene named levle2 (ArgumentError)", kPlatformer) == "the scenes are: main");
  CHECK(hint_for("no scene named mian (ArgumentError)", kPlatformer) == "did you mean: main");
  CHECK(hint_for("loading scripts/coins.rb: cannot read scripts/coins.rb", kPlatformer) == "did you mean: coin.rb");
  CHECK(has(hint_for("loading scripts/zzzzzzzz.rb: cannot read scripts/zzzzzzzz.rb", kPlatformer),
            "the scripts are: accounts.rb, camera_follow.rb"));
  CHECK(hint_for("something else went wrong", kPlatformer).empty());
}

TEST_CASE("redgem run says the hint after the error") {
  const fs::path game = fs::temp_directory_path() / "redgem_error_hints_game";
  fs::remove_all(game);
  fs::copy(kPlatformer, game, fs::copy_options::recursive);
  std::string error;
  REQUIRE(write_text_file(game / "scripts/goal.rb",
                          "class Goal < Behaviour\n  def start\n    transform.positon = Vector3.zero\n  end\nend\n",
                          error));
  const auto ran = run_tool_command({"run", game.string(), "--frames", "5"});
  REQUIRE(ran.has_value());
  CHECK(ran->exit_code == kExitFailure);
  CHECK(has(ran->out,
            "scripts/goal.rb:3: Goal#start on 'Goal' (behaviour disabled): undefined method 'positon=' for "
            "Transform (NoMethodError) -> did you mean: position=, position (redgem api Transform)"));
  fs::remove_all(game);
}

TEST_CASE("a method mistyped on one of the engine's modules is answered from the line that called it") {
  const fs::path game = fs::temp_directory_path() / "redgem_error_hints_module";
  fs::remove_all(game);
  fs::copy(kPlatformer, game, fs::copy_options::recursive);
  std::string error;
  REQUIRE(write_text_file(game / "scripts/goal.rb",
                          "class Goal < Behaviour\n  def start\n    Input.presed?(:space)\n  end\nend\n", error));
  const auto ran = run_tool_command({"run", game.string(), "--frames", "5"});
  REQUIRE(ran.has_value());
  CHECK(has(ran->out,
            "undefined method 'presed?' for Module (NoMethodError) -> did you mean: pressed? (redgem api Input)"));
  fs::remove_all(game);
  // Without a line to read, the calls of every class near it.
  CHECK(has(hint_for("undefined method 'presed?' for Module (NoMethodError)", kPlatformer), "Input.pressed?"));
}

TEST_CASE("a script looked for in a folder of scripts is answered with the paths there") {
  const fs::path game = fs::temp_directory_path() / "redgem_error_hints_folders";
  fs::remove_all(game);
  fs::copy(kPlatformer, game, fs::copy_options::recursive);
  std::string error;
  fs::create_directories(game / "scripts/enemies");
  REQUIRE(write_text_file(game / "scripts/enemies/slime.rb", "class Slime < Behaviour\nend\n", error));
  CHECK(hint_for("loading scripts/enemies/slim.rb: cannot read scripts/enemies/slim.rb", game) ==
        "did you mean: enemies/slime.rb");
  fs::remove_all(game);
}
