// MCP's terrain tool: a terrain's layers file read and written, checked as the game reads it, and
// painted by its rules.

#include <doctest/doctest.h>

#include <filesystem>
#include <stdexcept>
#include <string>

#include "project/mcp_tools.hpp"
#include "temp_dirs.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::tools;
using mcp::Json;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "terrain";
constexpr int kSplatSize = 16;

class GameCopy {
 public:
  GameCopy() {
    fs::remove_all(dir_);
    fs::copy(REDGEM_PLATFORMER_DIR, dir_, fs::copy_options::recursive);
  }
  ~GameCopy() { fs::remove_all(dir_); }
  GameCopy(const GameCopy&) = delete;
  GameCopy& operator=(const GameCopy&) = delete;

  const fs::path& dir() const { return dir_; }
  mcp::Options options(bool read_only = false) const { return {dir_, read_only, {}}; }
  Json call(const Json& args) const { return mcp::invoke(options(), kTool, args); }

 private:
  fs::path dir_ = test::temp_root() / "redgem_terrain_tool";
};

std::string text_of(const Json& result) { return result["content"][0]["text"]; }
bool has(const Json& result, const std::string& part) { return text_of(result).find(part) != std::string::npos; }

Json beach() {
  return {{"splat_size", kSplatSize},
          {"layers", {{{"name", "sand"}}, {{"name", "grass"}}}},
          {"rules", {{{"layer", "sand"}}, {{"layer", "grass"}, {"strength", 0.75}}}}};
}

Json write_beach(Json content = beach()) {
  return {{"action", "set_layers"}, {"name", "beach"}, {"content", std::move(content)}};
}

}  // namespace

TEST_CASE("terrain set_layers writes a layers file the game reads, and refuses one it cannot") {
  const GameCopy game;
  CHECK(text_of(game.call({{"action", "layers"}})).empty());
  Json broken = beach();
  broken["rules"] = {{{"layer", "snow"}}};
  CHECK_THROWS_WITH_AS(game.call(write_beach(broken)), doctest::Contains("not written"), std::invalid_argument);
  CHECK_FALSE(fs::exists(game.dir() / "terrains" / "beach.json"));
  CHECK(has(game.call(write_beach()), "terrains/beach.json"));
  CHECK(text_of(game.call({{"action", "layers"}})) == "beach\n");
  CHECK(has(game.call({{"action", "layers"}, {"name", "beach"}}), "grass"));
  CHECK_THROWS_WITH_AS(game.call(write_beach()), doctest::Contains("confirm=true"), std::invalid_argument);
  CHECK_THROWS_AS(game.call({{"action", "layers"}, {"name", "../scene"}}), std::invalid_argument);
}

TEST_CASE("terrain paint paints the layers an object names, and reads them by the object") {
  const GameCopy game;
  game.call(write_beach());
  CHECK(game.call({{"action", "paint"}, {"object", "Ground"}})["isError"] == true);  // it names no layers yet
  CHECK_THROWS_AS(game.call({{"action", "layers"}, {"object", "Ground"}}), std::invalid_argument);
  const Json named =
      mcp::invoke(game.options(), "scene_edit",
                  {{"action", "set"}, {"object", "Ground"}, {"assignments", {"prop.terrain_layers=beach"}}});
  REQUIRE_FALSE(named["isError"].get<bool>());
  CHECK(has(game.call({{"action", "layers"}, {"object", "Ground"}}), "terrains/beach.json"));
  const Json painted = game.call({{"action", "paint"}, {"object", "Ground"}});
  CHECK_FALSE(painted["isError"].get<bool>());
  CHECK(has(painted, "terrains/beach.splat0.png"));
  CHECK(fs::exists(game.dir() / "terrains" / "beach.splat0.png"));
  CHECK_THROWS_AS(game.call({{"action", "layers"}, {"object", "Ground"}, {"name", "beach"}}), std::invalid_argument);
  Json swapped = write_beach();
  swapped["confirm"] = true;
  swapped["content"]["layers"] = {{{"name", "grass"}}, {{"name", "sand"}}};
  CHECK(has(game.call(swapped), "paint it again"));
  CHECK_FALSE(fs::exists(game.dir() / "terrains" / "beach.splat0.png"));
}

TEST_CASE("terrain reads only the layers files of terrains/, whatever an object names") {
  const GameCopy game;
  const Json named =
      mcp::invoke(game.options(), "scene_edit",
                  {{"action", "set"}, {"object", "Ground"}, {"assignments", {"prop.terrain_layers=../game"}}});
  REQUIRE_FALSE(named["isError"].get<bool>());
  CHECK_THROWS_AS(game.call({{"action", "layers"}, {"object", "Ground"}}), std::invalid_argument);
}

TEST_CASE("read-only terrain only reads the layers") {
  const GameCopy game;
  CHECK_THROWS_AS(mcp::invoke(game.options(true), kTool, write_beach()), std::invalid_argument);
  CHECK_FALSE(fs::exists(game.dir() / "terrains"));
}
