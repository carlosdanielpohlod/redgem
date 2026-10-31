// The past conversations the Agent panel keeps for each game, and how it names and dates them.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <nlohmann/json.hpp>

#include "editor/agent/session_history.hpp"
#include "test_support.hpp"

using namespace redgem::agent;
namespace fs = std::filesystem;

namespace {

constexpr std::int64_t kSomeTime = 1'800'000'000;  // seconds since 1970

fs::path fresh_file(const std::string& name) {
  const fs::path folder = fs::temp_directory_path() / name;
  fs::remove_all(folder);
  fs::create_directories(folder);
  return folder / "agent-sessions.json";
}

std::vector<PastSession> remember(const fs::path& file, const fs::path& game, const PastSession& session) {
  std::string error;
  const auto kept = remember_session(file, game, session, error);
  REQUIRE_MESSAGE(kept.has_value(), error);
  return *kept;
}

// A file's whole text, closed again before the test removes its folder (Windows will not remove
// an open file).
std::string text_of(const fs::path& path) {
  std::ifstream in(path);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST_CASE("a game's past conversations are kept per game, newest first") {
  const fs::path file = fresh_file("redgem_sessions_per_game");
  const fs::path game = file.parent_path() / "game";
  const fs::path other = file.parent_path() / "other";
  CHECK(read_sessions(file, game).empty());
  remember(file, game, {"a", "Claude Code", "first", kSomeTime});
  remember(file, game, {"b", "Codex", "second", kSomeTime + 10});
  remember(file, other, {"c", "Claude Code", "elsewhere", kSomeTime + 20});
  const auto sessions = read_sessions(file, game);
  REQUIRE(sessions.size() == 2);
  CHECK(sessions[0].id == "b");
  CHECK(sessions[0].agent == "Codex");
  CHECK(sessions[1].title == "first");
  CHECK(read_sessions(file, other).size() == 1);
  CHECK(read_sessions(file, game / "." / "..").empty());                             // (another folder)
  CHECK(read_sessions(file, file.parent_path() / "x" / ".." / "game").size() == 2);  // (the same folder)
  fs::remove_all(file.parent_path());
}

TEST_CASE("using a conversation again moves it first, and only so many are kept") {
  const fs::path file = fresh_file("redgem_sessions_kept");
  const fs::path game = file.parent_path() / "game";
  for (std::size_t made = 0; made < kKeptSessions + 3; ++made) {
    remember(file, game, {"s" + std::to_string(made), "Claude Code", "t", kSomeTime + static_cast<std::int64_t>(made)});
  }
  remember(file, game, {"s3", "Claude Code", "t", kSomeTime + 1000});
  const auto sessions = read_sessions(file, game);
  REQUIRE(sessions.size() == kKeptSessions);
  CHECK(sessions.front().id == "s3");
  CHECK(sessions.back().id == "s4");  // (s0, s1 and s2 were let go)
  fs::remove_all(file.parent_path());
}

TEST_CASE("a file that cannot be read is kept aside, not written over") {
  const fs::path file = fresh_file("redgem_sessions_broken");
  const fs::path game = file.parent_path() / "game";
  const std::string broken = "{\"games\": [1, 2";
  std::ofstream(file) << broken;
  CHECK(read_sessions(file, game).empty());
  redgem::test::LogCapture quiet;
  remember(file, game, {"a", "Claude Code", "t", kSomeTime});
  CHECK(read_sessions(file, game).size() == 1);
  CHECK(text_of(file.string() + ".unreadable") == broken);
  // Built as JSON, so a Windows path's backslashes are escaped.
  const nlohmann::json kept_games = {
      {"games", {{fs::weakly_canonical(game).u8string(), {{{"id", 3}}, {{"id", "ok"}, {"agent", "Codex"}}}}}}};
  std::ofstream(file) << kept_games.dump();
  REQUIRE(read_sessions(file, game).size() == 1);
  CHECK(read_sessions(file, game)[0].id == "ok");
  fs::remove_all(file.parent_path());
}

TEST_CASE("what another editor kept meanwhile stays, and what is kept comes back from one write") {
  const fs::path file = fresh_file("redgem_sessions_two_editors");
  const fs::path game = file.parent_path() / "game";
  remember(file, game, {"mine", "Claude Code", "t", kSomeTime});
  remember(file, game, {"theirs", "Codex", "t", kSomeTime + 1});  // (the other editor, between two of mine)
  const auto kept = remember(file, game, {"mine", "Claude Code", "t", kSomeTime + 2});
  REQUIRE(kept.size() == 2);
  CHECK(kept[0].id == "mine");
  CHECK(kept[1].id == "theirs");
  CHECK(read_sessions(file, game).size() == 2);
  fs::remove_all(file.parent_path());
}

TEST_CASE("a conversation is titled by the first line of its first message") {
  CHECK(session_title("\n\nMake the coins spin\nand bob too") == "Make the coins spin");
  CHECK(session_title("") == "");
  const std::string long_line(kTitleLength + 10, 'x');
  CHECK(session_title(long_line) == std::string(kTitleLength, 'x') + "...");
  // (A character of two bytes that the cut would split is left out whole.)
  const std::string accented = std::string(kTitleLength - 1, 'a') + "\xc3\xa9" + "tail";
  CHECK(session_title(accented) == std::string(kTitleLength - 1, 'a') + "...");
}

TEST_CASE("how long ago a conversation was used") {
  CHECK(time_ago(kSomeTime, kSomeTime + 5) == "just now");
  CHECK(time_ago(kSomeTime, kSomeTime + 300) == "5 min ago");
  CHECK(time_ago(kSomeTime, kSomeTime + 7200) == "2 h ago");
  CHECK(time_ago(kSomeTime, kSomeTime + 3 * 86400) == "3 days ago");
  CHECK(time_ago(kSomeTime + 10, kSomeTime) == "just now");  // (a clock set back)
}
