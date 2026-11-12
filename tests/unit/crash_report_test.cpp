#include <doctest/doctest.h>

#include <csignal>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "app/crash_handlers.hpp"
#include "app/crash_report.hpp"
#include "app/crash_text.hpp"
#include "app/game_data_location.hpp"
#include "core/build_id.hpp"
#include "core/platform.hpp"

using namespace redgem;
using namespace redgem::crash;

namespace {

// Moments whose dates are known: the epoch, a leap day, the last second of a century's year.
constexpr long long kEpoch = 0;
constexpr long long kLeapDay2000 = 951782400;     // 2000-02-29 00:00:00 UTC
constexpr long long kEndOf1999 = 946684799;       // 1999-12-31 23:59:59 UTC
constexpr long long kSomeAfternoon = 1792847109;  // 2026-10-24 13:05:09 UTC
constexpr long long kBeforeEpoch = -1;            // 1969-12-31 23:59:59 UTC
constexpr std::size_t kTinyText = 4;

CrashFacts sample_facts() {
  CrashFacts facts;
  facts.game = "Arena";
  facts.version = "1.0.0";
  facts.platform = "Linux x86_64";
  facts.build_id = "3f2a9c";
  facts.program = "Arena";
  facts.time = "2026-10-24 13:05:09 UTC";
  facts.cause = "SIGSEGV (invalid memory access)";
  facts.backtrace = {"Arena+0x1234", "libc.so.6+0x29d90 (abort)"};
  return facts;
}

// A crashes folder of its own, empty, removed at the end.
struct ReportsFolder {
  std::filesystem::path path = std::filesystem::temp_directory_path() / "redgem-untold-crash-test";
  ReportsFolder() {
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
  }
  ~ReportsFolder() { std::filesystem::remove_all(path); }
  void add(const std::string& name) const { std::ofstream(path / name) << "RedGem crash report\n"; }
};

}  // namespace

TEST_CASE("moments are written in UTC without the C library's clock") {
  CHECK(std::string(moment_text(utc_moment(kEpoch)).c_str()) == "1970-01-01 00:00:00 UTC");
  CHECK(std::string(moment_text(utc_moment(kLeapDay2000)).c_str()) == "2000-02-29 00:00:00 UTC");
  CHECK(std::string(moment_text(utc_moment(kEndOf1999)).c_str()) == "1999-12-31 23:59:59 UTC");
  CHECK(std::string(moment_text(utc_moment(kSomeAfternoon)).c_str()) == "2026-10-24 13:05:09 UTC");
  CHECK(std::string(moment_text(utc_moment(kBeforeEpoch)).c_str()) == "1969-12-31 23:59:59 UTC");
  CHECK(std::string(moment_stamp(utc_moment(kSomeAfternoon)).c_str()) == "20261024-130509");
}

TEST_CASE("a fixed text keeps what fits and always ends") {
  FixedText<kTinyText> text;
  text.append("abcdef");
  CHECK(std::string(text.c_str()) == "abc");
  CHECK(text.size() == kTinyText - 1);
  FixedText<kTinyText> number;
  number.append_number(7, 3);
  CHECK(std::string(number.c_str()) == "007");
}

TEST_CASE("a report says what crashed, where it runs, when, why and where the program was") {
  const std::string report = format_crash_report(sample_facts());
  CHECK(report ==
        "RedGem crash report\n"
        "Game: Arena\n"
        "Engine: 1.0.0\n"
        "Platform: Linux x86_64\n"
        "Build: 3f2a9c Arena\n"
        "Time: 2026-10-24 13:05:09 UTC\n"
        "Cause: SIGSEGV (invalid memory access)\n"
        "Backtrace:\n"
        "  Arena+0x1234\n"
        "  libc.so.6+0x29d90 (abort)\n");
  CHECK(report.rfind(report_head(sample_facts()), 0) == 0);  // what a signal handler writes first
}

TEST_CASE("a report of a program with no build id says the build is unknown") {
  CrashFacts facts = sample_facts();
  facts.build_id.clear();
  CHECK(report_head(facts).find("\nBuild: unknown Arena\n") != std::string::npos);
}

TEST_CASE("a report names the build of the program it was written by") {
  const auto program = this_program();
  CHECK_FALSE(program.name.empty());
  CHECK(program.base != nullptr);
#if !defined(__APPLE__)
  // (The same id read from this test program's file: the loaded program and its file agree.)
  const auto from_file = binary_identity_of_file(platform::executable_path());
  REQUIRE(from_file.has_value());
  CHECK_FALSE(program.build_id.empty());
  CHECK(program.build_id == from_file->build_id);
#endif
}

TEST_CASE("the next run is told of the newest crash nobody was told of, once") {
  const ReportsFolder folder;
  CHECK_FALSE(untold_crash_report(folder.path).has_value());  // no report yet
  folder.add("crash-20261023-080000.log");
  folder.add("crash-20261024-130509.log");
  folder.add("notes.txt");
  const auto first = untold_crash_report(folder.path);
  REQUIRE(first.has_value());
  CHECK(first->filename() == "crash-20261024-130509.log");
  CHECK_FALSE(untold_crash_report(folder.path).has_value());  // told already
  folder.add("crash-20261025-090000.log");
  const auto next = untold_crash_report(folder.path);
  REQUIRE(next.has_value());
  CHECK(next->filename() == "crash-20261025-090000.log");
  CHECK_FALSE(untold_crash_report(std::filesystem::path("no-such-folder-for-crashes")).has_value());
}

TEST_CASE("a report with no backtrace says why") {
  CrashFacts facts = sample_facts();
  facts.backtrace.clear();
  CHECK(format_crash_report(facts).find("Backtrace:\n  (none: the stack had already unwound)\n") != std::string::npos);
}

TEST_CASE("crash reports go in the crashes folder beside the game's save, named after the moment") {
  const std::filesystem::path game = std::filesystem::temp_directory_path() / "redgem-crash-folder-test";
  std::filesystem::create_directories(game);
  std::ofstream(game / "game.json") << R"({"studio": "Pine Hill", "title": "Arena"})";
  const GameDataFiles files = game_data_files(players_data_folder(game));
  CHECK(crash_folder_for({game, {}, false}) == files.crashes);
  CHECK(crash_folder_for({game, {}, false}).parent_path() == files.save.parent_path());
  CHECK(crash_folder_for({game, "elsewhere", false}) == std::filesystem::path("elsewhere") / "crashes");
  CHECK(crash_file_name(kSomeAfternoon) == "crash-20261024-130509.log");
  std::filesystem::remove_all(game);
}

TEST_CASE("fatal signals are named in reports") {
  CHECK(std::string(signal_description(SIGSEGV)).rfind("SIGSEGV", 0) == 0);
  CHECK(std::string(signal_description(SIGABRT)).rfind("SIGABRT", 0) == 0);
  CHECK(std::string(signal_description(SIGFPE)).rfind("SIGFPE", 0) == 0);
  CHECK(std::string(signal_description(SIGILL)).rfind("SIGILL", 0) == 0);
}

TEST_CASE("a report names the engine's version and the platform") {
  CHECK(std::strlen(engine_version()) > 0);
  CHECK(std::string(platform_name()).find(' ') != std::string::npos);  // the system, then the processor
}

TEST_CASE("a fixed text writes numbers in hexadecimal") {
  constexpr std::uint64_t kOffset = 0x1a2b;
  FixedText<kTinyText * 4> text;
  text.append_hex(kOffset);
  text.append(' ');
  text.append_hex(0);
  CHECK(std::string(text.c_str()) == "1a2b 0");
}
