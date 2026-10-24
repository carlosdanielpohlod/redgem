#include <doctest/doctest.h>

#include <csignal>
#include <cstdint>
#include <cstring>
#include <string>

#include "app/crash_report.hpp"
#include "app/crash_text.hpp"

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
  facts.time = "2026-10-24 13:05:09 UTC";
  facts.cause = "SIGSEGV (invalid memory access)";
  facts.backtrace = {"redgem-player(+0x1234)", "libc.so.6(+0x29d90)"};
  return facts;
}

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
        "Time: 2026-10-24 13:05:09 UTC\n"
        "Cause: SIGSEGV (invalid memory access)\n"
        "Backtrace:\n"
        "  redgem-player(+0x1234)\n"
        "  libc.so.6(+0x29d90)\n");
  CHECK(report.rfind(report_head(sample_facts()), 0) == 0);  // what a signal handler writes first
}

TEST_CASE("a report with no backtrace says why") {
  CrashFacts facts = sample_facts();
  facts.backtrace.clear();
  CHECK(format_crash_report(facts).find("Backtrace:\n  (none: the stack had already unwound)\n") != std::string::npos);
}

TEST_CASE("crash reports go beside the game's save, named after it and after the moment") {
  const std::filesystem::path save = std::filesystem::path("data") / "saves" / "by-id" / "com.studio.arena.json";
  CHECK(crash_folder_for(save) == std::filesystem::path("data") / "saves" / "by-id" / "com.studio.arena-crashes");
  CHECK(crash_file_name(kSomeAfternoon) == "crash-20261024-130509.log");
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
