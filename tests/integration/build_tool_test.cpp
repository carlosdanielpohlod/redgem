// MCP's build tool: a build started on a thread of its own, followed with status. (This test
// program has no player beside it, so its builds fail, saying so; the stdio test of `redgem mcp`
// makes a real one.)

#include <doctest/doctest.h>

#include <filesystem>
#include <stdexcept>
#include <string>

#include "project/mcp_tools.hpp"
#include "test_support.hpp"

using namespace redgem::tools;
using mcp::Json;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "build";
constexpr int kWaitSeconds = 60;

// A copy of the sandbox game in a folder of its own, where its builds go too.
class GameCopy {
 public:
  GameCopy() {
    fs::remove_all(root_);
    fs::create_directories(root_);
    fs::copy(redgem::test::fixture("games/sandbox"), game(), fs::copy_options::recursive);
  }
  ~GameCopy() { fs::remove_all(root_); }
  GameCopy(const GameCopy&) = delete;
  GameCopy& operator=(const GameCopy&) = delete;

  fs::path root() const { return root_; }
  fs::path game() const { return root_ / "game"; }
  Json call(const Json& args) const { return mcp::invoke({game(), false, {}}, kTool, args); }

 private:
  fs::path root_ = fs::temp_directory_path() / "redgem_build_tool";
};

std::string text_of(const Json& result) { return result["content"][0]["text"]; }
bool has(const Json& result, const std::string& part) { return text_of(result).find(part) != std::string::npos; }
bool failed(const Json& result) { return result["isError"].get<bool>(); }

}  // namespace

TEST_CASE("build status says when no build was started, and start refuses what cannot be built") {
  const GameCopy copy;
  CHECK(has(copy.call({{"action", "status"}}), "no build was started"));
  CHECK_THROWS_AS(copy.call({{"action", "start"}, {"target", "linux"}, {"format", "bundle"}}), std::invalid_argument);
  CHECK_THROWS_AS(copy.call({{"action", "start"}, {"folder", "../away"}}), std::invalid_argument);
  CHECK_THROWS_AS(copy.call({{"action", "start"}, {"target", "consoles"}}), std::invalid_argument);
}

TEST_CASE("a build runs on its own thread, and status tells how it ended, then as the last build") {
  const GameCopy copy;
  const Json started = copy.call({{"action", "start"}, {"folder", "shipped"}});
  CHECK(has(started, "shipped"));
  const Json ended = copy.call({{"action", "status"}, {"wait", kWaitSeconds}});
  CHECK(failed(ended));
  CHECK(has(ended, "failed"));
  CHECK(has(ended, "redgem-player"));
  const Json last = copy.call({{"action", "status"}});
  CHECK(has(last, "last build"));
  CHECK_FALSE(fs::exists(copy.root() / "shipped" / "game.rgpak"));
}

TEST_CASE("a build into the game's own folder is refused when it starts") {
  const GameCopy copy;
  copy.call({{"action", "start"}, {"folder", "game"}});
  CHECK(failed(copy.call({{"action", "status"}, {"wait", kWaitSeconds}})));
}
