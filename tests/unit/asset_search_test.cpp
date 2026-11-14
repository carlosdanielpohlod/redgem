// Searching a game's assets and the templates by words, for assistants and the command line:
// what an asset is and is tagged, which matches come first, and what the filters and limits do.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/exit_codes.hpp"
#include "project/asset_search.hpp"
#include "project/mcp_tools.hpp"
#include "project/search_command.hpp"
#include "project/tool_commands.hpp"

using namespace redgem;
using namespace redgem::tools;
namespace fs = std::filesystem;

namespace {

// The files of a small game, every kind of asset, some in folders and some hidden.
const std::vector<std::string> kGameFiles = {
    "scene.glb",      "models/enemies/orc.glb", "models/orc_boss.glb", "models/.hidden.glb",   "models/.git/x.glb",
    "images/orc.png", "images/ui/button.png",   "sounds/hit.wav",      "scripts/orc_brain.rb", "prefabs/door.glb",
    "notes/orc.txt",
};
constexpr std::size_t kListedAssets = 7;  // the files above in an asset folder, not hidden
constexpr std::size_t kOrcMatches = 4;
constexpr std::size_t kOne = 1;

// A game of those files, gone with the test.
class SmallGame {
 public:
  explicit SmallGame(const std::string& name) : dir_(fs::temp_directory_path() / ("redgem_asset_search_" + name)) {
    fs::remove_all(dir_);
    for (const std::string& file : kGameFiles) {
      fs::create_directories((dir_ / file).parent_path());
      std::ofstream(dir_ / file) << file;
    }
  }
  ~SmallGame() { fs::remove_all(dir_); }
  SmallGame(const SmallGame&) = delete;
  SmallGame& operator=(const SmallGame&) = delete;

  const fs::path& dir() const { return dir_; }

 private:
  fs::path dir_;
};

std::vector<std::string> paths(const SearchResult<AssetEntry>& result) {
  std::vector<std::string> found;
  for (const AssetEntry& asset : result.found) found.push_back(asset.path);
  return found;
}

const AssetEntry& asset_at(const std::vector<AssetEntry>& assets, const std::string& path) {
  for (const AssetEntry& asset : assets) {
    if (asset.path == path) return asset;
  }
  FAIL("no asset " << path);
  return assets.front();
}

ToolResult tool(std::vector<std::string> words) {
  const auto result = run_tool_command(words);
  REQUIRE(result.has_value());
  return *result;
}

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("a word scores best as the whole name, then its start, inside it, a tag, and the rest") {
  const SearchFields orc{"Orc", {"enemies"}, "models/enemies/orc.glb"};
  const SearchFields boss{"orc_boss", {}, "models/orc_boss.glb"};
  const SearchFields brute{"brute_orc", {}, "models/brute_orc.glb"};
  const SearchFields tagged{"grunt", {"Orc"}, "models/orc/grunt.glb"};
  const SearchFields pathed{"grunt", {}, "models/orcish/grunt.glb"};
  CHECK(match_score(orc, "orc") > match_score(boss, "orc"));
  CHECK(match_score(boss, "orc") > match_score(brute, "orc"));
  CHECK(match_score(brute, "orc") > match_score(tagged, "orc"));
  CHECK(match_score(tagged, "orc") > match_score(pathed, "orc"));
  CHECK(match_score(pathed, "orc") > 0);
}

TEST_CASE("every word of a search must match, in any case, and no words match everything") {
  const SearchFields boss{"orc_boss", {"enemies"}, "models/enemies/orc_boss.glb"};
  CHECK(match_score(boss, "ORC Enemies") > match_score(boss, "orc"));
  CHECK(match_score(boss, "orc dragon") == 0);
  CHECK(match_score(boss, "") > 0);
  CHECK(match_score(boss, "   ") == match_score(boss, ""));
}

TEST_CASE("a game's assets are the files of its asset folders, kind by folder and tagged by subfolder") {
  const SmallGame game("listing");
  const std::vector<AssetEntry> assets = game_assets(game.dir());
  CHECK(assets.size() == kListedAssets);
  const AssetEntry orc = asset_at(assets, "models/enemies/orc.glb");
  CHECK(orc.kind == AssetKind::Model);
  CHECK(orc.tags == std::vector<std::string>{"enemies"});
  CHECK(asset_at(assets, "images/ui/button.png").kind == AssetKind::Texture);
  CHECK(asset_at(assets, "sounds/hit.wav").kind == AssetKind::Sound);
  CHECK(asset_at(assets, "scripts/orc_brain.rb").kind == AssetKind::Script);
  CHECK(asset_at(assets, "prefabs/door.glb").kind == AssetKind::Prefab);
  CHECK(asset_at(assets, "models/orc_boss.glb").tags.empty());
}

TEST_CASE("asset searches rank the best match first, then by path, and filter by kind and tag") {
  const SmallGame game("ranking");
  const std::vector<AssetEntry> assets = game_assets(game.dir());
  const auto orc = search_assets(assets, {"orc", std::nullopt, std::nullopt, kNoSearchLimit});
  CHECK(paths(orc) == std::vector<std::string>{"images/orc.png", "models/enemies/orc.glb", "models/orc_boss.glb",
                                               "scripts/orc_brain.rb"});
  CHECK(orc.matched == kOrcMatches);
  const auto models = search_assets(assets, {"orc", AssetKind::Model, std::nullopt, kNoSearchLimit});
  CHECK(paths(models) == std::vector<std::string>{"models/enemies/orc.glb", "models/orc_boss.glb"});
  const auto tagged = search_assets(assets, {"", std::nullopt, std::string("ENEMIES"), kNoSearchLimit});
  CHECK(paths(tagged) == std::vector<std::string>{"models/enemies/orc.glb"});
  const auto limited = search_assets(assets, {"orc", std::nullopt, std::nullopt, kOne});
  CHECK(limited.found.size() == kOne);
  CHECK(limited.matched == kOrcMatches);
}

TEST_CASE("kinds are named by one word each") {
  CHECK(kind_named("Texture") == AssetKind::Texture);
  CHECK_FALSE(kind_named("mesh").has_value());
  CHECK(std::string(kind_word(AssetKind::Prefab)) == "prefab");
  CHECK(kind_words("|") == "model|texture|sound|script|prefab");
}

TEST_CASE("template searches rank names before descriptions") {
  const std::vector<GameTemplate> templates = {
      {"racer", "Drive a car around a track", "car"},
      {"car-park", "Park between the lines", "car"},
      {"puzzle", "Slide tiles; no car", "grid"},
  };
  const auto found = search_templates(templates, "car", kNoSearchLimit);
  REQUIRE(found.found.size() == templates.size());
  CHECK(found.found[0].name == "car-park");
  CHECK(found.found[1].name == "puzzle");
  CHECK(found.found[2].name == "racer");
  CHECK(search_templates(templates, "boat", kNoSearchLimit).found.empty());
}

TEST_CASE("redgem assets and redgem templates answer a line per match") {
  const SmallGame game("command");
  const ToolResult models = tool({kAssetsCommand, game.dir().string(), "orc", "--kind", "model"});
  CHECK_MESSAGE(models.exit_code == kExitSuccess, models.err);
  CHECK(models.out == "model models/enemies/orc.glb\nmodel models/orc_boss.glb\n");
  const ToolResult limited = tool({kAssetsCommand, game.dir().string(), "orc", "--limit", "1"});
  CHECK(has(limited.out, "+3 more not shown"));
  CHECK(has(tool({kAssetsCommand, game.dir().string(), "dragon"}).out, "no asset matches"));
  CHECK(tool({kAssetsCommand, game.dir().string(), "--kind", "mesh"}).exit_code == kExitUsage);
  CHECK(tool({kAssetsCommand, game.dir().string(), "--limit", "1.5"}).exit_code == kExitUsage);
  CHECK(tool({kAssetsCommand, (game.dir() / "models").string()}).exit_code == kExitFailure);
  CHECK(tool({kAssetsCommand}).exit_code == kExitUsage);
  const ToolResult platformer = tool({kTemplatesCommand, "platformer"});
  CHECK(platformer.out.rfind("platformer: ", 0) == 0);
  CHECK(has(tool({kTemplatesCommand, "zzzz"}).out, "the templates are: "));
}

TEST_CASE("MCP asset_search and template_search answer from the game and the templates") {
  const SmallGame game("mcp");
  mcp::Options options;
  options.game = game.dir();
  options.read_only = true;
  const mcp::Json assets = mcp::invoke(options, "asset_search", {{"query", "door"}, {"kind", "prefab"}});
  CHECK_FALSE(assets["isError"].get<bool>());
  CHECK(assets["content"][0]["text"] == "prefab prefabs/door.glb\n");
  const mcp::Json templates = mcp::invoke(options, "template_search", {{"query", "platformer"}});
  CHECK(templates["content"][0]["text"].get<std::string>().rfind("platformer: ", 0) == 0);
  CHECK_THROWS_AS(mcp::invoke(options, "asset_search", {{"kind", "mesh"}}), std::invalid_argument);
}
