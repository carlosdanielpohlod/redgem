// `redgem scene paint`: a terrain's ground layers painted by the rules of its layers file, from
// the command line, and the weights written beside the file for the game and the editor to read.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/terrain_files.hpp"
#include "core/exit_codes.hpp"
#include "project/scene_command.hpp"
#include "project/tool_commands.hpp"
#include "scene/ground/splat_map.hpp"
#include "temp_dirs.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::tools;
namespace fs = std::filesystem;

namespace {

const fs::path kPlatformer = REDGEM_PLATFORMER_DIR;
constexpr int kMiddle = 8;  // of the 16 texels of the test's map
constexpr int kGrass = 1;

class GameCopy {
 public:
  explicit GameCopy(const std::string& name) : dir_(redgem::test::temp_root() / ("redgem_terrain_paint_" + name)) {
    fs::remove_all(dir_);
    fs::copy(kPlatformer, dir_, fs::copy_options::recursive);
    fs::create_directories(dir_ / "terrains");
  }
  ~GameCopy() { fs::remove_all(dir_); }
  GameCopy(const GameCopy&) = delete;
  GameCopy& operator=(const GameCopy&) = delete;

  std::string dir() const { return dir_.string(); }
  void write(const std::string& relative, const std::string& text) const { std::ofstream(dir_ / relative) << text; }

 private:
  fs::path dir_;
};

ToolResult scene_tool(std::vector<std::string> words) {
  words.insert(words.begin(), kSceneCommand);
  const auto result = run_tool_command(words);
  REQUIRE(result.has_value());
  return *result;
}

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

constexpr const char* kBeach = R"({"splat_size": 16,
  "layers": [{"name": "sand"}, {"name": "grass"}],
  "rules": [{"layer": "sand"}, {"layer": "grass", "strength": 0.75}]})";

void give_layers(const GameCopy& game, const char* file) {
  game.write("terrains/beach.json", file);
  const ToolResult named = scene_tool({"set", game.dir(), "Ground", "prop.terrain_layers=beach"});
  REQUIRE_MESSAGE(named.exit_code == kExitSuccess, named.err);
}

}  // namespace

TEST_CASE("scene paint writes the weights the rules of a terrain's layers file paint") {
  GameCopy game("rules");
  give_layers(game, kBeach);
  const ToolResult painted = scene_tool({"paint", game.dir(), "Ground"});
  REQUIRE_MESSAGE(painted.exit_code == kExitSuccess, painted.err);
  CHECK(has(painted.out, "texels of Ground by 2 rules: terrains/beach.splat0.png"));
  std::string error;
  const auto layers = read_terrain_files(game.dir(), "beach", TerrainReading::Weights, error);
  REQUIRE_MESSAGE(layers.has_value(), error);
  CHECK(texel_weights(layers->splat, kMiddle, kMiddle)[kGrass] == doctest::Approx(0.75f).epsilon(0.01));
}

TEST_CASE("scene paint refuses an object without layers, a file without rules, and other options") {
  GameCopy game("refused");
  const ToolResult no_layers = scene_tool({"paint", game.dir(), "Ground"});
  CHECK(no_layers.exit_code != kExitSuccess);
  CHECK(has(no_layers.err, "Ground has no ground layers"));
  give_layers(game, R"({"layers": [{"name": "sand"}]})");
  const ToolResult no_rules = scene_tool({"paint", game.dir(), "Ground"});
  CHECK(has(no_rules.err, "terrains/beach.json has no \"rules\""));
  const ToolResult option = scene_tool({"paint", game.dir(), "Ground", "--from", "Coin1"});
  CHECK(option.exit_code == kExitUsage);
  CHECK(has(option.err, "scene paint does not accept --from"));
  CHECK_FALSE(fs::exists(fs::path(game.dir()) / "terrains/beach.splat0.png"));
}
