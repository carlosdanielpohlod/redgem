// A program that crashes writes a report in its game's players' folder and ends with a failure. Each
// crash happens in a program of its own (tests/tools/crash_helper.cpp), which these tests start.

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/build_id.hpp"
#include "core/exit_codes.hpp"
#include "core/platform.hpp"
#include "project/symbolize_command.hpp"

namespace {

constexpr const char* kGameId = "com.tests.crashing";
constexpr const char* kGameTitle = "Crashing Game";

struct CrashFolders {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem-crash-test";
  std::filesystem::path data = root / "data";
  std::filesystem::path game = root / "game";
  // The game's players' folder holds its save, and its reports in crashes/.
  std::filesystem::path reports = data / "crashes";

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

TEST_CASE("running out of memory where nobody catches it is reported as such too") {
#if defined(_WIN32)
  // (Windows may hand it to the unhandled exception filter first, which cannot tell what it was.)
  check_reported("terminate-memory", "uncaught exception");
#else
  check_reported("terminate-memory", "out of memory");
#endif
}

TEST_CASE("an exception that leaves the work of an engine thread is reported with what it said") {
  check_reported("engine-thread", "uncaught exception: thrown on an engine thread");
  check_reported("engine-thread-memory", "out of memory");
}

#if !defined(__APPLE__)
namespace {

constexpr const char* kFunctionMark = "  in ";  // what a symbolized frame says its function after
constexpr const char* kUnknownFunction = "??";

std::string helper_name() { return std::filesystem::path(REDGEM_CRASH_HELPER_FILE).filename().u8string(); }

// Whether a symbolized report names a function at one of its frames (not addr2line's "??").
bool names_a_function(const std::string& text) {
  const std::size_t mark = std::string(kFunctionMark).size();
  for (std::size_t at = text.find(kFunctionMark); at != std::string::npos; at = text.find(kFunctionMark, at + 1)) {
    if (text.compare(at + mark, std::string(kUnknownFunction).size(), kUnknownFunction) != 0) return true;
  }
  return false;
}

}  // namespace

TEST_CASE("a report names the build that crashed by the id its program carries, and frames as offsets into it") {
  const CrashFolders folders;
  REQUIRE(crashed(folders, "segv"));
  const std::string report = the_report(folders);
  CAPTURE(report);
  const auto helper = redgem::binary_identity_of_file(REDGEM_CRASH_HELPER_FILE);
  REQUIRE(helper.has_value());
  CHECK_FALSE(helper->build_id.empty());
  CHECK(says(report, "\nBuild: " + helper->build_id + " " + helper_name() + "\n"));
  CHECK(says(report, "\n  " + helper_name() + "+0x"));
}

TEST_CASE("a crash's report is symbolized with the program that crashed, by addr2line") {
  const auto tool = redgem::tools::addr2line_on_path(redgem::platform::environment("PATH"));
  if (!tool.has_value()) {
    // (The continuous integration's machines have one: there the test must run.)
    CHECK(redgem::platform::environment("CI").empty());
    return;
  }
  const CrashFolders folders;
  REQUIRE(crashed(folders, "segv"));
  const std::filesystem::path symbols = folders.root / "symbols";
  std::filesystem::create_directories(symbols);
  std::filesystem::copy_file(REDGEM_CRASH_HELPER_FILE, symbols / helper_name());
  const auto result =
      redgem::tools::symbolize_report(the_report(folders), symbols, redgem::tools::addr2line_resolver(*tool));
  CAPTURE(result.out);
  CAPTURE(result.err);
  CHECK(result.exit_code == redgem::kExitSuccess);
  CHECK(names_a_function(result.out));
}
#endif
