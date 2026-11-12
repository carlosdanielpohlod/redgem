#pragma once

// `redgem symbolize <report> [--symbols <file or folder>] [--tool <addr2line>]`: a crash report
// with the functions and file:line of its frames. The frames of a shipped game's report are
// offsets into its program, which was stripped of its symbols; the symbols of every release's
// players are kept apart (redgem-symbols-<system>.zip), and of a build of one's own in its build
// folder. The file whose build id is the report's is found among them, and llvm-addr2line (or
// binutils' addr2line) says what is at each offset. A report whose build no file there matches
// is said to be so, with the builds that are there.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "project/crash_symbols.hpp"
#include "project/tool_line.hpp"

namespace redgem::tools {

inline constexpr const char* kSymbolizeCommand = "symbolize";

ToolResult run_symbolize_command(const std::vector<std::string>& args);

// The same for a report's text, with the symbols in `symbols` (a file, or a folder of them) and
// addresses looked up by `resolve`.
ToolResult symbolize_report(const std::string& report, const std::filesystem::path& symbols,
                            const AddressResolver& resolve);

// Looks addresses up with a program of the addr2line kind.
AddressResolver addr2line_resolver(const std::filesystem::path& tool);
// The first llvm-addr2line on a PATH (the folders in `path_variable`), else the first addr2line.
std::optional<std::filesystem::path> addr2line_on_path(const std::string& path_variable);

}  // namespace redgem::tools
