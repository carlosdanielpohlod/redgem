// MCP's project tool: game.json, the locales, dialogues and quests, scenes and new games, read and
// written as files, each write checked as `redgem check` checks the game.

#include <doctest/doctest.h>

#include <filesystem>
#include <stdexcept>
#include <string>

#include "core/config.hpp"
#include "core/files.hpp"
#include "project/mcp_tools.hpp"

using namespace redgem;
using namespace redgem::tools;
using mcp::Json;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "project";

// A copy of the platformer of the test's own, in a folder of its own (new games go next to it).
class GameCopy {
 public:
  GameCopy() {
    fs::remove_all(root_);
    fs::create_directories(root_);
    fs::copy(REDGEM_PLATFORMER_DIR, game(), fs::copy_options::recursive);
  }
  ~GameCopy() { fs::remove_all(root_); }
  GameCopy(const GameCopy&) = delete;
  GameCopy& operator=(const GameCopy&) = delete;

  fs::path root() const { return root_; }
  fs::path game() const { return root_ / "game"; }
  mcp::Options options(bool read_only = false) const { return {game(), read_only, {}}; }
  Json call(const Json& args) const { return mcp::invoke(options(), kTool, args); }

 private:
  fs::path root_ = fs::temp_directory_path() / "redgem_project_tools";
};

std::string text_of(const Json& result) { return result["content"][0]["text"]; }
bool has(const Json& result, const std::string& part) { return text_of(result).find(part) != std::string::npos; }

std::string file_text(const fs::path& file) {
  std::string error;
  return read_text_file(file, error).value_or("");
}

}  // namespace

TEST_CASE("project settings reads game.json, and set_settings merges into it or refuses what breaks it") {
  const GameCopy copy;
  CHECK(has(copy.call({{"action", "settings"}}), "\"network\": true"));
  const Json title{{"action", "set_settings"}, {"changes", {{"window", {{"title", "Coast"}}}, {"network", nullptr}}}};
  CHECK(has(copy.call(title), "\"title\": \"Coast\""));
  const Json saved = Json::parse(file_text(copy.game() / "game.json"));
  CHECK(saved["window"]["title"] == "Coast");
  CHECK_FALSE(saved.contains("network"));
  const std::string before = file_text(copy.game() / "game.json");
  const Json bad{{"action", "set_settings"}, {"changes", {{"input", {{"actions", {{"jump", {"key:nope"}}}}}}}}};
  CHECK_THROWS_WITH_AS(copy.call(bad), doctest::Contains("not written"), std::invalid_argument);
  CHECK(file_text(copy.game() / "game.json") == before);
  CHECK_THROWS_AS(copy.call({{"action", "set_settings"}}), std::invalid_argument);
}

TEST_CASE("project reads and writes the menu theme, refusing what no theme takes") {
  const GameCopy copy;
  const Json button{{"menu", {{"theme", {{"colors", {{"button", "#203040"}}}}}}}};
  copy.call({{"action", "set_settings"}, {"changes", button}});
  CHECK(has(copy.call({{"action", "settings"}, {"section", "menu"}}), "#203040"));
  const Json wrong_key{{"menu", {{"theme", {{"colours", {{"button", "#203040"}}}}}}}};
  CHECK_THROWS_WITH_AS(copy.call({{"action", "set_settings"}, {"changes", wrong_key}}),
                       doctest::Contains("not written"), std::invalid_argument);
  CHECK_FALSE(has(copy.call({{"action", "settings"}, {"section", "menu"}}), "colours"));
  CHECK_THROWS_AS(copy.call({{"action", "settings"}, {"section", "nothing"}}), std::invalid_argument);
}

TEST_CASE("project texts lists a language's texts by prefix, bounded, and set_texts adds and removes them") {
  const GameCopy copy;
  const Json hud = copy.call({{"action", "texts"}, {"prefix", "hud."}});
  CHECK(has(hud, "hud.coins = Coins {coins}/{total}"));
  CHECK_FALSE(has(hud, "banner.win"));
  CHECK(has(copy.call({{"action", "texts"}, {"prefix", "hud."}, {"limit", 1}}), "(1 shown"));
  const Json add{{"action", "set_texts"}, {"texts", {{"hud.lives", "Lives {lives}"}}}};
  CHECK(has(copy.call(add), "texts"));
  CHECK(has(copy.call({{"action", "texts"}, {"prefix", "hud.lives"}}), "hud.lives = Lives {lives}"));
  const Json gone{{"action", "set_texts"}, {"texts", {{"hud.lives", nullptr}}}};
  copy.call(gone);
  CHECK(has(copy.call({{"action", "texts"}, {"prefix", "hud.lives"}}), "0 texts"));
  // The tree's dialogue shows tree.hello: taking its text away is refused.
  const Json needed{{"action", "set_texts"}, {"texts", {{"tree.hello", nullptr}}}};
  CHECK_THROWS_WITH_AS(copy.call(needed), doctest::Contains("tree.hello"), std::invalid_argument);
  CHECK(has(copy.call({{"action", "texts"}, {"prefix", "tree.hello"}}), "1 texts"));
  CHECK_THROWS_AS(copy.call({{"action", "set_texts"}, {"texts", {{"hud.x", 3}}}}), std::invalid_argument);
  CHECK_THROWS_AS(copy.call({{"action", "texts"}, {"language", "../en"}}), std::invalid_argument);
}

TEST_CASE("project story lists, reads and writes dialogues and quests, refusing broken ones") {
  const GameCopy copy;
  CHECK(text_of(copy.call({{"action", "story"}, {"kind", "dialogue"}})) == "tree\n");
  CHECK(has(copy.call({{"action", "story"}, {"kind", "quest"}, {"name", "tree"}}), "quest.tree.title"));
  CHECK_THROWS_AS(copy.call({{"action", "story"}, {"kind", "dialogue"}, {"name", "nobody"}}), std::invalid_argument);
  const Json greeting{{"start", "hi"},
                      {"nodes", {{"hi", {{"speaker", "tree.name"}, {"text", "tree.hello"}, {"end", true}}}}}};
  const Json write{{"action", "set_story"}, {"kind", "dialogue"}, {"name", "npcs/baker"}, {"content", greeting}};
  CHECK(has(copy.call(write), "dialogues/npcs/baker.json"));
  CHECK(text_of(copy.call({{"action", "story"}, {"kind", "dialogue"}})) == "npcs/baker\ntree\n");
  CHECK_THROWS_WITH_AS(copy.call(write), doctest::Contains("confirm=true"), std::invalid_argument);
  Json broken = write;
  broken["confirm"] = true;
  broken["content"]["start"] = "nowhere";
  CHECK_THROWS_WITH_AS(copy.call(broken), doctest::Contains("not written"), std::invalid_argument);
  CHECK(Json::parse(file_text(copy.game() / "dialogues/npcs/baker.json"))["start"] == "hi");
  Json fresh = broken;
  fresh["name"] = "ghost";
  CHECK_THROWS_AS(copy.call(fresh), std::invalid_argument);
  CHECK_FALSE(fs::exists(copy.game() / "dialogues/ghost.json"));
  Json outside = write;
  outside["name"] = "../escape";
  CHECK_THROWS_AS(copy.call(outside), std::invalid_argument);
}

TEST_CASE("project scenes lists them and new_scene makes one") {
  const GameCopy copy;
  CHECK(text_of(copy.call({{"action", "scenes"}})) == "main\n");
  CHECK(has(copy.call({{"action", "new_scene"}, {"scene", "cave"}}), "scenes/cave.glb"));
  CHECK(fs::exists(copy.game() / config::kScenesDirName / "cave.glb"));
  CHECK(has(copy.call({{"action", "scenes"}}), "cave"));
  CHECK_THROWS_AS(copy.call({{"action", "new_scene"}, {"scene", "cave"}}), std::invalid_argument);
}

TEST_CASE("project new_game makes a game next to this one, and only there") {
  const GameCopy copy;
  CHECK(has(copy.call({{"action", "new_game"}, {"path", "second"}}), "second"));
  CHECK(fs::exists(copy.root() / "second" / "game.json"));
  CHECK_THROWS_AS(copy.call({{"action", "new_game"}, {"path", "second"}}), std::invalid_argument);
  CHECK_THROWS_AS(copy.call({{"action", "new_game"}, {"path", "../away"}}), std::invalid_argument);
  CHECK_THROWS_AS(copy.call({{"action", "new_game"}, {"path", "third"}, {"template", "nope"}}), std::invalid_argument);
}

TEST_CASE("read-only project only reads") {
  const GameCopy copy;
  const auto options = copy.options(true);
  CHECK(has(mcp::invoke(options, kTool, {{"action", "settings"}}), "network"));
  CHECK_THROWS_AS(mcp::invoke(options, kTool, {{"action", "new_scene"}, {"scene", "cave"}}), std::invalid_argument);
  CHECK_FALSE(fs::exists(copy.game() / config::kScenesDirName));
}
