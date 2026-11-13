// `redgem scene diff <game> --against <revision>`: the scene compared with the same scene as a
// Git commit saved it, in a repository of the test's own; and what it says when there is no such
// revision, or no repository at all.

#include <doctest/doctest.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/encoder_process.hpp"
#include "core/exit_codes.hpp"
#include "core/platform.hpp"
#include "project/mcp_tools.hpp"
#include "project/scene_command.hpp"
#include "project/scene_revision.hpp"
#include "project/tool_commands.hpp"

using namespace redgem;
using namespace redgem::tools;
namespace fs = std::filesystem;

namespace {

const fs::path kPlatformer = REDGEM_PLATFORMER_DIR;
// Git does not look for a repository above the folders this names (the test's temp folder).
constexpr const char* kGitCeiling = "GIT_CEILING_DIRECTORIES";
constexpr const char* kGitOutputSuffix = "_git.txt";

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// Runs git in `folder`; whether it ended well.
bool git(const fs::path& folder, std::vector<std::string> words) {
  words.insert(words.begin(), {"git", "-C", folder.string(), "-c", "user.name=RedGemTests", "-c",
                               "user.email=tests@example.invalid", "-c", "commit.gpgsign=false"});
  const fs::path output = folder.parent_path() / (folder.filename().string() + kGitOutputSuffix);
  const ProcessEnd end = run_to_end(words, output);
  fs::remove(output);
  return end.started && end.exited && end.code == kExitSuccess;
}

// Git looks for no repository above the temp folder while this lasts (one there must not be
// taken for a game's).
class CeilingAtTemp {
 public:
  CeilingAtTemp() : before_(platform::environment(kGitCeiling)) {
    platform::set_environment(kGitCeiling, fs::temp_directory_path().string(), true);
  }
  ~CeilingAtTemp() { platform::set_environment(kGitCeiling, before_, true); }
  CeilingAtTemp(const CeilingAtTemp&) = delete;
  CeilingAtTemp& operator=(const CeilingAtTemp&) = delete;

 private:
  std::string before_;
};

// A copy of the platformer of the test's own, in a repository of its own when asked, with its
// scene committed; gone with the test.
class GameCopy {
 public:
  GameCopy(const std::string& name, bool in_git) : dir_(fs::temp_directory_path() / ("redgem_scene_revision_" + name)) {
    fs::remove_all(dir_);
    fs::copy(kPlatformer, dir_, fs::copy_options::recursive);
    if (in_git) commit_scene();
  }
  ~GameCopy() {
    std::error_code ignored;
    fs::remove_all(dir_, ignored);
  }
  GameCopy(const GameCopy&) = delete;
  GameCopy& operator=(const GameCopy&) = delete;

  std::string dir() const { return dir_.string(); }
  fs::path scene() const { return dir_ / "scene.glb"; }

 private:
  void commit_scene() {
    REQUIRE(git(dir_, {"init", "-q"}));
    REQUIRE(git(dir_, {"add", "scene.glb"}));
    REQUIRE(git(dir_, {"commit", "-q", "-m", "first"}));
  }

  CeilingAtTemp ceiling_;
  fs::path dir_;
};

ToolResult scene_tool(std::vector<std::string> words) {
  words.insert(words.begin(), kSceneCommand);
  const auto result = run_tool_command(words);
  REQUIRE(result.has_value());
  return *result;
}

}  // namespace

TEST_CASE("a revision is a branch, tag or commit with ~ and ^, never an option, a range, a path or a file") {
  for (const char* good : {"HEAD", "HEAD~1", "HEAD^2", "main", "v1.2.0", "feature/door", "3fa2c1b", "main@{1}"}) {
    CHECK_MESSAGE(valid_revision(good), good);
  }
  for (const char* bad :
       {"", "--output=x", "-p", "main..dev", "HEAD:scene.glb", "a b", "a;b", "a\"b", "old.glb", "$(touch x)"}) {
    CHECK_FALSE_MESSAGE(valid_revision(bad), bad);
  }
  CHECK_FALSE(valid_revision(std::string(kMostRevisionBytes + 1, 'a')));
  CHECK(names_scene_file("before.glb"));
  CHECK(names_scene_file("Before.GLB"));
  CHECK_FALSE(names_scene_file("HEAD~1"));
}

TEST_CASE("scene diff against a Git revision compares with the scene that commit saved") {
  const GameCopy game("committed", true);
  const ToolResult same = scene_tool({"diff", game.dir(), "--against", "HEAD"});
  CHECK_MESSAGE(same.exit_code == kExitSuccess, same.err);
  CHECK(has(same.out, "no object changes"));
  REQUIRE(scene_tool({"set", game.dir(), "Coin1", "position=1,2,3"}).exit_code == kExitSuccess);
  const ToolResult changed = scene_tool({"diff", game.dir(), "--against", "HEAD"});
  CHECK_MESSAGE(changed.exit_code == kExitSuccess, changed.err);
  CHECK(has(changed.out, "+0 -0 ~1 objects"));
  CHECK(has(changed.out, "~ #7 Coin1"));
  REQUIRE(git(game.dir(), {"commit", "-q", "-a", "-m", "second"}));
  CHECK(has(scene_tool({"diff", game.dir(), "--against", "HEAD"}).out, "no object changes"));
  CHECK(has(scene_tool({"diff", game.dir(), "--against", "HEAD~1"}).out, "~ #7 Coin1"));
  CHECK(has(scene_tool({"diff", game.dir(), "--revision", "HEAD~1"}).out, "~ #7 Coin1"));
}

TEST_CASE("scene diff --against takes a file that is there for an earlier save, whatever its name") {
  const GameCopy game("backup", true);
  const fs::path backup = fs::path(game.dir()) / "scene_backup";
  fs::copy_file(game.scene(), backup);
  REQUIRE(scene_tool({"set", game.dir(), "Coin1", "position=1,2,3"}).exit_code == kExitSuccess);
  const ToolResult diff = scene_tool({"diff", game.dir(), "--against", backup.string()});
  CHECK_MESSAGE(diff.exit_code == kExitSuccess, diff.err);
  CHECK(has(diff.out, "~ #7 Coin1"));
  CHECK(scene_tool({"diff", game.dir(), "--against", backup.string(), "--revision", "HEAD"}).exit_code == kExitUsage);
  CHECK(scene_tool({"list", game.dir(), "--revision", "HEAD"}).exit_code == kExitUsage);
}

TEST_CASE("scene diff says when the revision or the repository is not there") {
  const GameCopy committed("missing_revision", true);
  const ToolResult missing = scene_tool({"diff", committed.dir(), "--against", "no-such-branch"});
  CHECK(missing.exit_code == kExitFailure);
  CHECK(has(missing.err, "Git has no scene.glb at no-such-branch"));
  CHECK(scene_tool({"diff", committed.dir(), "--against", "--output=x"}).exit_code == kExitUsage);
  CHECK(scene_tool({"diff", committed.dir(), "--against", "main..dev"}).exit_code == kExitUsage);
  const GameCopy loose("no_repository", false);
  const ToolResult outside = scene_tool({"diff", loose.dir(), "--against", "HEAD"});
  CHECK(outside.exit_code == kExitFailure);
  CHECK(has(outside.err, "is in no Git repository"));
}

TEST_CASE("MCP scene_diff takes a revision or an earlier save, one of them") {
  const GameCopy game("mcp", true);
  mcp::Options options;
  options.game = game.dir();
  options.read_only = true;
  const mcp::Json same = mcp::invoke(options, "scene_diff", {{"revision", "HEAD"}});
  CHECK_FALSE(same["isError"].get<bool>());
  CHECK(has(same["content"][0]["text"].get<std::string>(), "no object changes"));
  CHECK_THROWS_AS(mcp::invoke(options, "scene_diff", {{"revision", "HEAD"}, {"against", "scene.glb"}}),
                  std::invalid_argument);
  CHECK_THROWS_AS(mcp::invoke(options, "scene_diff", mcp::Json::object()), std::invalid_argument);
  CHECK_THROWS_AS(mcp::invoke(options, "scene_diff", {{"revision", "--output=x"}}), std::invalid_argument);
  CHECK_THROWS_AS(mcp::invoke(options, "scene_diff", {{"against", "scripts/coin.rb"}}), std::invalid_argument);
  CHECK(mcp::invoke(options, "scene_diff", {{"revision", "no-such-branch"}})["isError"] == true);
  CHECK_THROWS_AS(mcp::invoke(options, "asset_search", {{"query", "--help"}}), std::invalid_argument);
}
