// A program that crashes writes a report beside its game's save and ends with a failure. Each
// crash happens in a program of its own (tests/tools/crash_helper.cpp), which these tests start.

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/exit_codes.hpp"

namespace {

constexpr const char* kGameId = "com.tests.crashing";
constexpr const char* kGameTitle = "Crashing Game";

struct CrashFolders {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem-crash-test";
  std::filesystem::path data = root / "data";
  std::filesystem::path game = root / "game";
  // Where the game's save is (saves/by-id/<id>.json), its reports go: beside it.
  std::filesystem::path reports = data / "saves" / "by-id" / (std::string(kGameId) + "-crashes");

  CrashFolders() {
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(game);
    std::ofstream(game / "game.json") << "{\"id\": \"" << kGameId << "\", \"title\": \"" << kGameTitle << "\"}";
  }
  ~CrashFolders() { std::filesystem::remove_all(root); }
};

std::string quoted(const std::filesystem::path& path) { return "\"" + path.string() + "\""; }

// Runs the helper; whether it ended with a failure.
bool crashed(const CrashFolders& folders, const char* how) {
  std::string command =
      quoted(REDGEM_CRASH_HELPER_FILE) + " " + quoted(folders.data) + " " + quoted(folders.game) + " " + how;
#if defined(_WIN32)
  command = "\"" + command + "\"";  // (cmd takes off the outer quotes of a line that has more)
#endif
  return std::system(command.c_str()) != redgem::kExitSuccess;
}

// What the one report written says; empty when there is not exactly one.
std::string the_report(const CrashFolders& folders) {
  std::error_code error;
  std::filesystem::directory_iterator files(folders.reports, error);
  if (error || files == std::filesystem::directory_iterator()) return {};
  const std::filesystem::path file = files->path();
  if (++files != std::filesystem::directory_iterator()) return {};
  std::ostringstream text;
  text << std::ifstream(file).rdbuf();
  return text.str();
}

bool says(const std::string& report, const std::string& what) { return report.find(what) != std::string::npos; }

void check_reported(const char* how, const std::string& cause) {
  CAPTURE(how);
  const CrashFolders folders;
  CHECK(crashed(folders, how));
  const std::string report = the_report(folders);
  CAPTURE(report);
  CHECK(says(report, "RedGem crash report"));
  CHECK(says(report, std::string("Game: ") + kGameTitle));
  CHECK(says(report, "Engine: "));
  CHECK(says(report, "Platform: "));
  CHECK(says(report, " UTC\n"));
  CHECK(says(report, "Cause: " + cause));
  CHECK(says(report, "Backtrace:\n  "));
}

}  // namespace

TEST_CASE("an invalid memory access is reported beside the save, with where the program was") {
#if defined(_WIN32)
  check_reported("segv", "EXCEPTION_ACCESS_VIOLATION");
#else
  check_reported("segv", "SIGSEGV");
#endif
}

TEST_CASE("an abort is reported") { check_reported("abort", "SIGABRT"); }

TEST_CASE("an exception nobody can catch is reported, with what it said") {
#if defined(_WIN32)
  // (Windows may hand it to the unhandled exception filter first, which cannot read its message.)
  check_reported("terminate", "uncaught exception");
#else
  check_reported("terminate", "uncaught exception: thrown where nobody catches it");
#endif
}

TEST_CASE("an exception that reaches the program's top is reported") {
  check_reported("exception", "uncaught exception: thrown out of the game");
}

TEST_CASE("running out of memory is reported as such") { check_reported("memory", "out of memory"); }
