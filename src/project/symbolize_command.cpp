#include "project/symbolize_command.hpp"

#include <array>
#include <sstream>

#include "app/encoder_process.hpp"
#include "core/command_line.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"
#include "project/tool_play.hpp"

namespace redgem::tools {
namespace {

constexpr const char* kSymbolsOption = "--symbols";
constexpr const char* kToolOption = "--tool";
constexpr const char* kPathVariable = "PATH";
#ifdef _WIN32
constexpr char kPathListSeparator = ';';
#else
constexpr char kPathListSeparator = ':';
#endif
// The tools that look addresses up, the first one found used: LLVM's (which reads the programs
// of every system), then binutils'.
constexpr std::array<const char*, 2> kAddr2LineTools = {"llvm-addr2line", "addr2line"};
// How each is asked: each address said before what is said of it (-a), its function (-f), the
// function's name readable (-C), in a file (-e).
constexpr const char* kAddressesFlag = "-a";
constexpr const char* kFunctionsFlag = "-f";
constexpr const char* kDemangleFlag = "-C";
constexpr const char* kFileFlag = "-e";
constexpr const char* kHexPrefix = "0x";
constexpr const char* kAnswerFile = "addr2line.txt";
constexpr const char* kCurrentFolder = ".";
constexpr auto kRunnable =
    std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;

// What a program's file is called here, past its name: nothing, or Windows' .exe.
std::vector<std::string> program_extensions() { return {platform::executable_file_name("", platform::is_windows())}; }

ToolGrammar grammar() { return {{kSymbolsOption, kToolOption}, {command_line::kHelpOption}}; }

ToolResult answer(std::string text) { return ToolResult{kExitSuccess, std::move(text), {}}; }

ToolResult usage_failure(const std::string& problem) {
  return failed(problem + "\n\n" + tr("project.symbolize.usage"), kExitUsage);
}

// The builds a folder holds, a line each ("  <id>  <file>"), for a report that matches none of them.
std::string builds_found(const std::vector<SymbolFile>& files) {
  if (files.empty()) return tr("project.symbolize.no_builds_there");
  std::string lines = tr("project.symbolize.builds_there");
  for (const SymbolFile& file : files) lines += "\n  " + file.identity.build_id + "  " + file.file.u8string();
  return lines;
}

ToolResult no_symbols(const ReportBuild& build, const std::filesystem::path& place,
                      const std::vector<SymbolFile>& files) {
  const std::string message =
      tr("project.symbolize.no_symbols",
         {{"place", place.u8string()}, {"build_id", build.build_id}, {"program", build.program}});
  return failed(message + "\n" + builds_found(files));
}

// Whether a file may be run as a program here (on Windows, every file found by its .exe may).
bool runnable_program(const std::filesystem::path& file) {
  std::error_code fs_error;
  if (!std::filesystem::is_regular_file(file, fs_error)) return false;
  if (platform::is_windows()) return true;
  return (std::filesystem::status(file, fs_error).permissions() & kRunnable) != std::filesystem::perms::none;
}

std::optional<std::filesystem::path> chosen_tool(const ToolLine& line) {
  if (const auto named = line.value(kToolOption)) return std::filesystem::u8path(*named);
  return addr2line_here();
}

std::filesystem::path symbols_place(const ToolLine& line) {
  return std::filesystem::u8path(line.value(kSymbolsOption).value_or(kCurrentFolder));
}

std::string hex_address(std::uint64_t address) {
  std::ostringstream text;
  text << kHexPrefix << std::hex << address;
  return text.str();
}

bool ended_well(const ProcessEnd& end) { return end.started && end.exited && end.code == kExitSuccess; }

std::vector<ResolvedAddress> look_up(const std::filesystem::path& tool, const std::filesystem::path& symbols,
                                     const std::vector<std::uint64_t>& addresses) {
  std::vector<std::string> arguments = {tool.string(), kAddressesFlag, kFunctionsFlag,
                                        kDemangleFlag, kFileFlag,      symbols.string()};
  for (const std::uint64_t address : addresses) arguments.push_back(hex_address(address));
  const ScratchFolder scratch;  // (removed, with what the tool said in it, when this ends)
  const std::filesystem::path output = scratch.path() / kAnswerFile;
  const ProcessEnd end = run_to_end(arguments, output);
  std::string error;
  const std::string said = read_text_file(output, error).value_or("");
  return ended_well(end) ? addr2line_answer(said) : std::vector<ResolvedAddress>();
}

}  // namespace

ToolResult run_symbolize_command(const std::vector<std::string>& args) {
  std::string error;
  const auto line = read_tool_line(args, 0, grammar(), error);
  if (!line) return usage_failure(error);
  if (line->has(command_line::kHelpOption)) return answer(tr("project.symbolize.usage"));
  if (line->words.size() != 1) return usage_failure(tr("project.symbolize.one_report"));
  const auto report = read_text_file(std::filesystem::u8path(line->words.front()), error);
  if (!report) return failed(error);
  const auto tool = chosen_tool(*line);
  if (!tool) return failed(tr("project.symbolize.no_tool", {{"option", kToolOption}}));
  return symbolize_report(*report, symbols_place(*line), addr2line_resolver(*tool));
}

ToolResult symbolize_report(const std::string& report, const std::filesystem::path& symbols,
                            const AddressResolver& resolve) {
  const auto build = report_build(report);
  if (!build) return failed(tr("project.symbolize.no_build"));
  if (names_no_build(*build)) return failed(tr("project.symbolize.unknown_build", {{"program", build->program}}));
  const std::vector<SymbolFile> files = symbol_files_in(symbols);
  const SymbolFile* matching = symbols_of_build(files, build->build_id);
  if (matching == nullptr) return no_symbols(*build, symbols, files);
  const auto symbolized = symbolized_report(report, *build, *matching, resolve);
  if (!symbolized) return failed(tr("project.symbolize.tool_failed", {{"symbols", matching->file.u8string()}}));
  return answer(*symbolized);
}

AddressResolver addr2line_resolver(const std::filesystem::path& tool) {
  return [tool](const std::filesystem::path& symbols, const std::vector<std::uint64_t>& addresses) {
    return look_up(tool, symbols, addresses);
  };
}

std::optional<std::filesystem::path> addr2line_on_path(const std::string& path_variable,
                                                       const agent::ProgramTest& runnable) {
  for (const char* tool : kAddr2LineTools) {
    std::filesystem::path found =
        agent::find_program_in(tool, path_variable, kPathListSeparator, program_extensions(), runnable);
    if (!found.empty()) return found;
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> addr2line_here() {
  return addr2line_on_path(platform::environment(kPathVariable), runnable_program);
}

}  // namespace redgem::tools
