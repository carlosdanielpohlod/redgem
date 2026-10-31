// The past conversations the Agent panel keeps for each game, and how it names and dates them.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "editor/agent/session_history.hpp"

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

void remember(const fs::path& file, const fs::path& game, const PastSession& session) {
  std::string error;
  REQUIRE_MESSAGE(remember_session(file, game, session, error), error);
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

TEST_CASE("a broken file reads as no conversations, and is written anew") {
  const fs::path file = fresh_file("redgem_sessions_broken");
  const fs::path game = file.parent_path() / "game";
  std::ofstream(file) << "{\"games\": [1, 2";
  CHECK(read_sessions(file, game).empty());
  remember(file, game, {"a", "Claude Code", "t", kSomeTime});
  CHECK(read_sessions(file, game).size() == 1);
  std::ofstream(file) << R"({"games": {")" << fs::weakly_canonical(game).u8string()
                      << R"(": [{"id": 3}, {"id": "ok", "agent": "Codex"}]}})";
  REQUIRE(read_sessions(file, game).size() == 1);
  CHECK(read_sessions(file, game)[0].id == "ok");
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
