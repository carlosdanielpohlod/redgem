#include "project/crash_symbols.hpp"

#include <algorithm>
#include <cstring>

#include "app/crash_handlers.hpp"
#include "app/crash_report.hpp"

namespace redgem::tools {
namespace {

constexpr char kLineBreak = '\n';
constexpr char kCarriageReturn = '\r';  // (a report passed through Windows' Notepad has them)
constexpr char kFieldSeparator = ' ';
constexpr char kHiddenMark = '.';  // a folder named so (.git, .deps) holds no symbols worth reading
constexpr const char* kHexDigits = "0123456789abcdefABCDEF";
constexpr const char* kHexPrefix = "0x";
constexpr int kHexBase = 16;
constexpr std::size_t kMaxOffsetDigits = 16;  // of a 64-bit number
constexpr const char* kUnknownMark = "??";    // what addr2line says where it does not know
constexpr const char* kFunctionMark = "  in ";
constexpr const char* kLocationMark = " at ";
// What addr2line -a says of each address: the address, its function, its file:line.
constexpr std::size_t kAnswerLines = 3;

std::vector<std::string> lines_of(const std::string& text) {
  std::vector<std::string> lines(1);
  for (const char letter : text) {
    if (letter == kLineBreak) {
      lines.emplace_back();
    } else if (letter != kCarriageReturn) {
      lines.back().push_back(letter);
    }
  }
  return lines;
}

std::string joined_lines(const std::vector<std::string>& lines) {
  std::string text;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) text += kLineBreak;
    text += lines[i];
  }
  return text;
}

bool starts_with(const std::string& text, const char* start) { return text.rfind(start, 0) == 0; }

// Whether a frame's place ends at `end`, or what the loader named the frame starts there.
bool frame_ends_at(const std::string& place, std::size_t end) {
  return end == place.size() || place.compare(end, std::strlen(crash::kSymbolMark), crash::kSymbolMark) == 0;
}

// Whether the offset's digits (from `at` to `end` of a frame's place) are a number that ends the frame.
bool ends_a_frame(const std::string& place, std::size_t at, std::size_t end) {
  const bool has_digits = end > at && end - at <= kMaxOffsetDigits;
  return has_digits && frame_ends_at(place, end);
}

// The line of a report's first frame: where the program was when it crashed. Every frame after it
// is where a call returns to, one byte past the call, which is what is looked up for it.
std::size_t first_frame_line(const std::vector<std::string>& lines) {
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (report_frame(lines[i])) return i;
  }
  return lines.size();
}

// Whether a frame's offset is where the program was itself, not where a call returns to.
bool is_exact(const ReportFrame& frame, bool first) { return first || frame.offset == 0; }

std::uint64_t looked_up_offset(const ReportFrame& frame, bool first) {
  return is_exact(frame, first) ? frame.offset : frame.offset - 1;
}

// The lines of a report that are frames of a program, and the addresses they are at in its symbols.
struct FramesToLookUp {
  std::vector<std::size_t> lines;
  std::vector<std::uint64_t> addresses;
};

FramesToLookUp frames_of_program(const std::vector<std::string>& lines, const std::string& program,
                                 std::uint64_t base) {
  FramesToLookUp found;
  const std::size_t first = first_frame_line(lines);
  for (std::size_t i = first; i < lines.size(); ++i) {
    const auto frame = report_frame(lines[i]);
    if (!frame) continue;
    if (frame->program != program) continue;
    found.lines.push_back(i);
    found.addresses.push_back(base + looked_up_offset(*frame, i == first));
  }
  return found;
}

bool says_nothing(const ResolvedAddress& resolved) { return resolved.function.empty() && resolved.location.empty(); }

// What follows a frame: "  in <function> at <file:line>", as much of it as the symbols say.
std::string described(const ResolvedAddress& resolved) {
  if (says_nothing(resolved)) return {};
  std::string text = kFunctionMark + (resolved.function.empty() ? std::string(kUnknownMark) : resolved.function);
  if (!resolved.location.empty()) text += kLocationMark + resolved.location;
  return text;
}

// What addr2line said, or nothing where it said it does not know.
std::string known(const std::string& said) { return starts_with(said, kUnknownMark) ? std::string() : said; }

// Whether a line of addr2line's is the address it answers for (what it says of it follows).
bool is_address_line(const std::string& line) {
  const std::size_t digits_at = std::strlen(kHexPrefix);
  const bool has_digits = starts_with(line, kHexPrefix) && line.size() > digits_at;
  return has_digits && line.find_first_not_of(kHexDigits, digits_at) == std::string::npos;
}

// Adds a file that is a program carrying a build id.
void add_if_symbols(std::vector<SymbolFile>& files, const std::filesystem::path& file) {
  const auto identity = binary_identity_of_file(file);
  if (!identity) return;
  if (identity->build_id.empty()) return;
  std::error_code fs_error;
  files.push_back({file, *identity, std::filesystem::file_size(file, fs_error)});
}

// Whether a file is larger than another (than none at all): of two copies of a build, the one
// stripped is the smaller.
bool is_larger(const SymbolFile& file, const SymbolFile* other) {
  return other == nullptr || file.bytes > other->bytes;
}

bool is_hidden_folder(const std::filesystem::directory_entry& entry) {
  std::error_code fs_error;
  return entry.is_directory(fs_error) && entry.path().filename().u8string().front() == kHiddenMark;
}

}  // namespace

std::optional<ReportBuild> report_build(const std::string& report) {
  for (const std::string& line : lines_of(report)) {
    if (!starts_with(line, crash::kBuildLabel)) continue;
    const std::string rest = line.substr(std::strlen(crash::kBuildLabel));
    const std::size_t space = rest.find(kFieldSeparator);
    if (space == std::string::npos) return ReportBuild{rest, {}};
    return ReportBuild{rest.substr(0, space), rest.substr(space + 1)};
  }
  return std::nullopt;
}

bool names_no_build(const ReportBuild& build) {
  return build.build_id.empty() || build.build_id == crash::kUnknownBuild;
}

std::optional<ReportFrame> report_frame(const std::string& line) {
  if (!starts_with(line, crash::kFrameIndent)) return std::nullopt;
  const std::string place = line.substr(std::strlen(crash::kFrameIndent));
  const std::size_t mark = place.rfind(crash::kOffsetMark);
  if (mark == std::string::npos) return std::nullopt;
  const std::size_t digits_at = mark + std::strlen(crash::kOffsetMark);
  const std::size_t digits_end = std::min(place.find_first_not_of(kHexDigits, digits_at), place.size());
  if (!ends_a_frame(place, digits_at, digits_end)) return std::nullopt;
  const std::string digits = place.substr(digits_at, digits_end - digits_at);
  return ReportFrame{place.substr(0, mark), std::stoull(digits, nullptr, kHexBase)};
}

std::vector<SymbolFile> symbol_files_in(const std::filesystem::path& place) {
  std::vector<SymbolFile> files;
  std::error_code fs_error;
  if (std::filesystem::is_regular_file(place, fs_error)) add_if_symbols(files, place);
  const auto options = std::filesystem::directory_options::skip_permission_denied;
  for (std::filesystem::recursive_directory_iterator at(place, options, fs_error), end; at != end;
       at.increment(fs_error)) {
    if (is_hidden_folder(*at)) at.disable_recursion_pending();
    if (at->is_regular_file(fs_error)) add_if_symbols(files, at->path());
  }
  return files;
}

const SymbolFile* symbols_of_build(const std::vector<SymbolFile>& files, const std::string& build_id) {
  const SymbolFile* largest = nullptr;
  for (const SymbolFile& file : files) {
    if (file.identity.build_id != build_id) continue;
    if (is_larger(file, largest)) largest = &file;
  }
  return largest;
}

std::optional<std::string> symbolized_report(const std::string& report, const ReportBuild& build,
                                             const SymbolFile& symbols, const AddressResolver& resolve) {
  std::vector<std::string> lines = lines_of(report);
  const FramesToLookUp frames = frames_of_program(lines, build.program, symbols.identity.preferred_base);
  if (frames.addresses.empty()) return joined_lines(lines);
  const std::vector<ResolvedAddress> resolved = resolve(symbols.file, frames.addresses);
  if (resolved.empty()) return std::nullopt;
  const std::size_t count = std::min(frames.lines.size(), resolved.size());
  for (std::size_t i = 0; i < count; ++i) lines[frames.lines[i]] += described(resolved[i]);
  return joined_lines(lines);
}

std::vector<ResolvedAddress> addr2line_answer(const std::string& output) {
  const std::vector<std::string> lines = lines_of(output);
  std::vector<ResolvedAddress> answer;
  for (std::size_t i = 0; i + kAnswerLines <= lines.size(); ++i) {
    if (!is_address_line(lines[i])) continue;
    answer.push_back({known(lines[i + 1]), known(lines[i + kAnswerLines - 1])});
    i += kAnswerLines - 1;
  }
  return answer;
}

}  // namespace redgem::tools
