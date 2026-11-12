#include "project/symbolize_command.hpp"

#include <array>
#include <chrono>
#include <sstream>

#include "app/encoder_process.hpp"
#include "core/command_line.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"

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
// How each is asked: the function of each address (-f), its name readable (-C), in a file (-e).
constexpr const char* kFunctionsFlag = "-f";
constexpr const char* kDemangleFlag = "-C";
constexpr const char* kFileFlag = "-e";
constexpr const char* kHexPrefix = "0x";
constexpr const char* kScratchPrefix = "redgem-symbolize-";
constexpr const char* kScratchExtension = ".txt";
constexpr const char* kCurrentFolder = ".";

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

bool is_program_in(const std::string& folder, const std::filesystem::path& candidate) {
  std::error_code fs_error;
  return !folder.empty() && std::filesystem::is_regular_file(candidate, fs_error);
}

std::optional<std::filesystem::path> file_on_path(const std::string& file, const std::string& path_variable) {
  std::istringstream folders(path_variable);
  for (std::string folder; std::getline(folders, folder, kPathListSeparator);) {
    const std::filesystem::path candidate = std::filesystem::u8path(folder) / file;
    if (is_program_in(folder, candidate)) return candidate;
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> chosen_tool(const ToolLine& line) {
  if (const auto named = line.value(kToolOption)) return std::filesystem::u8path(*named);
  return addr2line_on_path(platform::environment(kPathVariable));
}

std::filesystem::path symbols_place(const ToolLine& line) {
  return std::filesystem::u8path(line.value(kSymbolsOption).value_or(kCurrentFolder));
}

std::string hex_address(std::uint64_t address) {
  std::ostringstream text;
  text << kHexPrefix << std::hex << address;
  return text.str();
}

// A file of its own for what one run of the tool says.
std::filesystem::path scratch_file() {
  const auto moment = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() / (kScratchPrefix + std::to_string(moment) + kScratchExtension);
}

bool ended_well(const ProcessEnd& end) { return end.started && end.exited && end.code == kExitSuccess; }

std::vector<ResolvedAddress> look_up(const std::filesystem::path& tool, const std::filesystem::path& symbols,
                                     const std::vector<std::uint64_t>& addresses) {
  std::vector<std::string> arguments = {tool.string(), kFunctionsFlag, kDemangleFlag, kFileFlag, symbols.string()};
  for (const std::uint64_t address : addresses) arguments.push_back(hex_address(address));
  const std::filesystem::path output = scratch_file();
  const ProcessEnd end = run_to_end(arguments, output);
  std::string error;
  const std::string said = read_text_file(output, error).value_or("");
  std::error_code fs_error;
  std::filesystem::remove(output, fs_error);
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
  return answer(symbolized_report(report, *build, *matching, resolve));
}

AddressResolver addr2line_resolver(const std::filesystem::path& tool) {
  return [tool](const std::filesystem::path& symbols, const std::vector<std::uint64_t>& addresses) {
    return look_up(tool, symbols, addresses);
  };
}

std::optional<std::filesystem::path> addr2line_on_path(const std::string& path_variable) {
  for (const char* tool : kAddr2LineTools) {
    const auto found = file_on_path(platform::executable_file_name(tool, platform::is_windows()), path_variable);
    if (found) return found;
  }
  return std::nullopt;
}

}  // namespace redgem::tools
