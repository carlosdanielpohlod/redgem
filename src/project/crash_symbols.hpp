#pragma once

// What `redgem symbolize` reads a crash report for, and how it puts names to the report's frames:
// the build that crashed (the report's "Build:" line), the frames of its backtrace that are
// offsets into that build's program ("Arena.exe+0x1a2b"), the file of symbols kept of the same
// build (found by its build id, core/build_id.hpp), and what a tool of the addr2line kind says
// of each address. Nothing here runs a program: how addresses are looked up is handed in.

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/build_id.hpp"

namespace redgem::tools {

// What a report says of the build that crashed (see app/crash_report.hpp).
struct ReportBuild {
  std::string build_id;  // crash::kUnknownBuild when the program carried none
  std::string program;   // as the frames of the backtrace name it
};
// Nothing when the report has no "Build:" line (not a crash report, or one of a RedGem before them).
std::optional<ReportBuild> report_build(const std::string& report);
// Whether the build cannot be matched to symbols: the report names no id.
bool names_no_build(const ReportBuild& build);

// A frame of a report's backtrace, "  <program>+0x<offset>", maybe with " (<symbol>)" after it.
struct ReportFrame {
  std::string program;
  std::uint64_t offset = 0;
};
// Nothing for a line that is no such frame.
std::optional<ReportFrame> report_frame(const std::string& line);

// What the symbols say of an address: its function and its file:line, each empty when they do not.
struct ResolvedAddress {
  std::string function;
  std::string location;
};
// Looks up addresses (as the symbols' own file counts them) in a file of symbols: an answer for
// each, in order. Fewer answers than addresses leave the last frames as they are.
using AddressResolver = std::function<std::vector<ResolvedAddress>(const std::filesystem::path& symbols,
                                                                   const std::vector<std::uint64_t>& addresses)>;

// A file of symbols: a program of a build as it was linked, before it was stripped.
struct SymbolFile {
  std::filesystem::path file;
  BinaryIdentity identity;
  std::uintmax_t bytes = 0;
};
// The programs that carry a build id: the file itself, or every one in a folder and the folders in
// it (but hidden ones, such as .git).
std::vector<SymbolFile> symbol_files_in(const std::filesystem::path& place);
// The one of a build; null when none is. Of several (a copy stripped of its symbols keeps its build
// id), the largest.
const SymbolFile* symbols_of_build(const std::vector<SymbolFile>& files, const std::string& build_id);

// The report, each frame of the build's program followed by what the symbols say of it:
//   "  Arena.exe+0x1a2b  in redgem::Engine::step() at src/app/engine.cpp:120"
// The first frame is looked up where it is; every other one a byte before, inside the call it
// returns from. Nothing when the resolver answered nothing (it could not read the symbols).
std::optional<std::string> symbolized_report(const std::string& report, const ReportBuild& build,
                                             const SymbolFile& symbols, const AddressResolver& resolve);

// What addr2line answers when asked with -a -f: for each address the address, a line with its
// function, then one with its file:line ("??" and "??:0" where it does not know). Lines that are
// none of those (what it says of its own troubles) are passed over.
std::vector<ResolvedAddress> addr2line_answer(const std::string& output);

}  // namespace redgem::tools
