// `redgem symbolize`: reading a crash report's build and frames, finding the symbols of that build,
// and putting what a resolver says beside each frame (a resolver of the tests' own: no addr2line).

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/exit_codes.hpp"
#include "core/platform.hpp"
#include "project/crash_symbols.hpp"
#include "project/symbolize_command.hpp"

using namespace redgem;
using namespace redgem::tools;

namespace {

constexpr const char* kBuildId = "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf01000000";
constexpr const char* kOtherBuildId = "0123456789abcdef";
constexpr std::uint64_t kImageBase = 0x140000000;
constexpr std::uint64_t kStepOffset = 0x1a2b;
constexpr std::uint64_t kMainOffset = 0x10;
#ifdef _WIN32
constexpr const char* kPathListSeparator = ";";
#else
constexpr const char* kPathListSeparator = ":";
#endif

std::string report_of(const std::string& build_line) {
  return "RedGem crash report\n"
         "Game: Arena\n"
         "Engine: 0.6.0\n"
         "Platform: Windows x86_64\n" +
         build_line +
         "Time: 2026-10-24 13:05:09 UTC\n"
         "Cause: EXCEPTION_ACCESS_VIOLATION (invalid memory access)\n"
         "Backtrace:\n"
         "  Arena.exe+0x1a2b\n"
         "  ntdll.dll+0x5000\n"
         "  Arena.exe+0x10\n";
}

const std::string kReport = report_of(std::string("Build: ") + kBuildId + " Arena.exe\n");

template <typename Value>
void put(std::vector<unsigned char>& bytes, std::size_t at, Value value) {
  if (bytes.size() < at + sizeof value) bytes.resize(at + sizeof value);
  std::memcpy(&bytes[at], &value, sizeof value);
}

// The smallest PE32+ file with a build id (kBuildId) and an image base (kImageBase): the symbols
// of the report's build, as far as finding them goes.
std::vector<unsigned char> symbols_picture() {
  std::vector<unsigned char> bytes;
  put<std::uint16_t>(bytes, 0, 0x5a4d);     // MZ
  put<std::uint32_t>(bytes, 0x3c, 0x80);    // the PE header
  put<std::uint32_t>(bytes, 0x80, 0x4550);  // PE
  put<std::uint16_t>(bytes, 0x86, 1);       // one section
  put<std::uint16_t>(bytes, 0x94, 240);     // the optional header's size
  put<std::uint16_t>(bytes, 0x98, 0x20b);   // PE32+
  put<std::uint64_t>(bytes, 0x98 + 24, kImageBase);
  put<std::uint32_t>(bytes, 0x98 + 108, 16);      // directories
  put<std::uint32_t>(bytes, 0x98 + 160, 0x2000);  // the debug directory
  put<std::uint32_t>(bytes, 0x98 + 164, 28);
  put<std::uint32_t>(bytes, 0x188 + 12, 0x2000);  // the section: loaded at 0x2000, 0x200 bytes at 0x400
  put<std::uint32_t>(bytes, 0x188 + 16, 0x200);
  put<std::uint32_t>(bytes, 0x188 + 20, 0x400);
  put<std::uint32_t>(bytes, 0x400 + 12, 2);  // CodeView, at 0x500 in the file
  put<std::uint32_t>(bytes, 0x400 + 24, 0x500);
  put<std::uint32_t>(bytes, 0x500, 0x53445352);  // RSDS
  for (std::uint32_t i = 0; i < 16; ++i) put<std::uint8_t>(bytes, 0x504 + i, static_cast<std::uint8_t>(0xa0 + i));
  put<std::uint32_t>(bytes, 0x514, 1);  // its age
  return bytes;
}

// A folder of symbols with the report's build in it (as redgem-player.exe, in a folder of its own)
// and a file that is no program.
struct SymbolsFolder {
  std::filesystem::path path = std::filesystem::temp_directory_path() / "redgem-symbolize-test";
  std::filesystem::path player = path / "windows" / "redgem-player.exe";

  SymbolsFolder() {
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(player.parent_path());
    const std::vector<unsigned char> bytes = symbols_picture();
    std::ofstream(player, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    std::ofstream(path / "README.txt") << "symbols";
  }
  ~SymbolsFolder() { std::filesystem::remove_all(path); }
};

// A resolver that notes what it was asked and answers what it was told to.
struct FakeResolver {
  std::filesystem::path asked_file;
  std::vector<std::uint64_t> asked;
  std::vector<ResolvedAddress> answers;

  AddressResolver resolver() {
    return [this](const std::filesystem::path& file, const std::vector<std::uint64_t>& addresses) {
      asked_file = file;
      asked = addresses;
      return answers;
    };
  }
};

bool says(const std::string& text, const std::string& what) { return text.find(what) != std::string::npos; }

}  // namespace

TEST_CASE("a report's build line names the build id and the program") {
  const auto build = report_build(kReport);
  REQUIRE(build.has_value());
  CHECK(build->build_id == kBuildId);
  CHECK(build->program == "Arena.exe");
  CHECK_FALSE(names_no_build(*build));
  const auto spaced = report_build(report_of("Build: 3f2a My Game.exe\n"));
  REQUIRE(spaced.has_value());
  CHECK(spaced->program == "My Game.exe");
  CHECK(names_no_build(*report_build(report_of("Build: unknown redgem-player\n"))));
  CHECK_FALSE(report_build(report_of("")).has_value());
}

TEST_CASE("a frame is an offset into a program, maybe with the loader's name of it after") {
  const auto plain = report_frame("  Arena.exe+0x1a2b");
  REQUIRE(plain.has_value());
  CHECK(plain->program == "Arena.exe");
  CHECK(plain->offset == kStepOffset);
  const auto named = report_frame("  My Game+0x10 (main)");
  REQUIRE(named.has_value());
  CHECK(named->program == "My Game");
  CHECK(named->offset == kMainOffset);
  CHECK(report_frame("  libredgem.so+0xDEADbeef")->offset == 0xdeadbeef);
  CHECK_FALSE(report_frame("Game: Arena").has_value());
  CHECK_FALSE(report_frame("  redgem-player(+0x1234) [0x55d4]").has_value());  // the C library's own way
  CHECK_FALSE(report_frame("  Arena.exe+0x").has_value());
  CHECK_FALSE(report_frame("  Arena.exe+0x12zz").has_value());
}

TEST_CASE("what addr2line answers is read three lines to an address, its own troubles passed over") {
  const auto answer = addr2line_answer(
      "0x140001a2b\r\nmain\r\n/src/app/player_main.cpp:58\r\n"
      "llvm-addr2line: warning: no debug info\n"
      "0x0000000140005000\n??\n??:0\n0x14000000f\nstep()\n??:?\n");
  REQUIRE(answer.size() == 3);
  CHECK(answer[0].function == "main");
  CHECK(answer[0].location == "/src/app/player_main.cpp:58");
  CHECK(answer[1].function.empty());
  CHECK(answer[1].location.empty());
  CHECK(answer[2].function == "step()");
  CHECK(answer[2].location.empty());
}

TEST_CASE("the frames of the build's program are looked up in its symbols, at the symbols' addresses") {
  const SymbolsFolder folder;
  FakeResolver fake;
  fake.answers = {{"redgem::Engine::step()", "src/app/engine.cpp:120"}, {"main", ""}};
  const ToolResult result = symbolize_report(kReport, folder.path, fake.resolver());
  CHECK(result.exit_code == kExitSuccess);
  CHECK(fake.asked_file == folder.player);
  // (The first frame where it crashed; the next one inside the call it returns from, a byte before.)
  const std::vector<std::uint64_t> expected = {kImageBase + kStepOffset, kImageBase + kMainOffset - 1};
  CHECK(fake.asked == expected);
  CHECK(says(result.out, "  Arena.exe+0x1a2b  in redgem::Engine::step() at src/app/engine.cpp:120\n"));
  CHECK(says(result.out, "\n  ntdll.dll+0x5000\n"));  // another program's frame, as it was
  CHECK(says(result.out, "\n  Arena.exe+0x10  in main\n"));
  CHECK(says(result.out, "Cause: EXCEPTION_ACCESS_VIOLATION"));
  // (The file of symbols itself does as well as its folder.)
  CHECK(symbolize_report(kReport, folder.player, fake.resolver()).exit_code == kExitSuccess);
}

TEST_CASE("frames the resolver knows nothing of are left as they were") {
  const SymbolsFolder folder;
  FakeResolver fake;
  fake.answers = {{"", ""}};
  const ToolResult result = symbolize_report(kReport, folder.path, fake.resolver());
  CHECK(result.exit_code == kExitSuccess);
  CHECK(says(result.out, "\n  Arena.exe+0x1a2b\n"));
  CHECK(says(result.out, "\n  Arena.exe+0x10\n"));
}

TEST_CASE("a resolver that answers nothing (an addr2line that failed) fails the command") {
  const SymbolsFolder folder;
  FakeResolver fake;
  const ToolResult result = symbolize_report(kReport, folder.path, fake.resolver());
  CHECK(result.exit_code == kExitFailure);
  CHECK(says(result.err, "redgem-player"));
}

TEST_CASE("of two files of the same build, the larger is the one with symbols") {
  const BinaryIdentity identity{kBuildId, kImageBase};
  constexpr std::uintmax_t kStrippedBytes = 100;
  constexpr std::uintmax_t kFullBytes = 900;
  const std::vector<SymbolFile> files = {{"stripped/Arena.exe", identity, kStrippedBytes},
                                         {"symbols/Arena.exe", identity, kFullBytes},
                                         {"other/Arena.exe", {kOtherBuildId, kImageBase}, kFullBytes * 2}};
  const SymbolFile* chosen = symbols_of_build(files, kBuildId);
  REQUIRE(chosen != nullptr);
  CHECK(chosen->file == std::filesystem::path("symbols/Arena.exe"));
  CHECK(symbols_of_build(files, "feed") == nullptr);
}

TEST_CASE("a report of another build is said to match no symbols, with the builds there are") {
  const SymbolsFolder folder;
  FakeResolver fake;
  const ToolResult result = symbolize_report(report_of(std::string("Build: ") + kOtherBuildId + " Arena.exe\n"),
                                             folder.path, fake.resolver());
  CHECK(result.exit_code == kExitFailure);
  CHECK(says(result.err, kOtherBuildId));
  CHECK(says(result.err, kBuildId));  // the build that is there
  CHECK(says(result.err, "redgem-symbols-"));
  CHECK(fake.asked.empty());
  const ToolResult empty = symbolize_report(kReport, folder.path / "windows-nothing", fake.resolver());
  CHECK(empty.exit_code == kExitFailure);
}

TEST_CASE("a report that names no build, or an unknown one, cannot be symbolized") {
  const SymbolsFolder folder;
  FakeResolver fake;
  CHECK(symbolize_report(report_of(""), folder.path, fake.resolver()).exit_code == kExitFailure);
  const ToolResult unknown = symbolize_report(report_of("Build: unknown Arena\n"), folder.path, fake.resolver());
  CHECK(unknown.exit_code == kExitFailure);
  CHECK(says(unknown.err, "Arena"));
}

TEST_CASE("redgem symbolize's command line: usage errors end with 2, what cannot be read with 1") {
  CHECK(run_symbolize_command({}).exit_code == kExitUsage);
  CHECK(run_symbolize_command({"one.log", "two.log"}).exit_code == kExitUsage);
  CHECK(run_symbolize_command({"--unknown", "one.log"}).exit_code == kExitUsage);
  CHECK(run_symbolize_command({"one.log", "--symbols"}).exit_code == kExitUsage);
  const ToolResult help = run_symbolize_command({"--help"});
  CHECK(help.exit_code == kExitSuccess);
  CHECK(says(help.out, "redgem symbolize <report>"));
  CHECK(run_symbolize_command({"no-such-report.log", "--tool", "addr2line"}).exit_code == kExitFailure);
}

TEST_CASE("the addr2line used is LLVM's when the PATH has it, else binutils'") {
  const std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem-addr2line-test";
  const std::filesystem::path gnu = root / "gnu";
  const std::filesystem::path llvm = root / "llvm";
  std::filesystem::create_directories(gnu);
  std::filesystem::create_directories(llvm);
  const std::string gnu_tool = platform::executable_file_name("addr2line", platform::is_windows());
  const std::string llvm_tool = platform::executable_file_name("llvm-addr2line", platform::is_windows());
  std::ofstream(gnu / gnu_tool) << "tool";
  std::ofstream(llvm / llvm_tool) << "tool";
  const auto any_file = [](const std::filesystem::path& file) { return std::filesystem::is_regular_file(file); };
  CHECK(addr2line_on_path(gnu.string() + kPathListSeparator + llvm.string(), any_file) == llvm / llvm_tool);
  CHECK(addr2line_on_path(kPathListSeparator + gnu.string(), any_file) == gnu / gnu_tool);
  CHECK_FALSE(addr2line_on_path("", any_file).has_value());
  std::filesystem::remove_all(root);
}
