#include "editor/agent/text_diff.hpp"

#include <algorithm>
#include <sstream>

namespace redgem::agent {
namespace {

using Lines = std::vector<std::string>;

Lines split_lines(const std::string& text) {
  Lines lines;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) lines.push_back(line);
  return lines;
}

// How many lines both start with.
std::size_t common_start(const Lines& before, const Lines& after) {
  const std::size_t most = std::min(before.size(), after.size());
  std::size_t count = 0;
  for (; count < most; ++count) {
    if (before[count] != after[count]) break;
  }
  return count;
}

// How many lines both end with, of those after their common start.
std::size_t common_end(const Lines& before, const Lines& after, std::size_t start) {
  const std::size_t most = std::min(before.size(), after.size()) - start;
  std::size_t count = 0;
  for (; count < most; ++count) {
    if (before[before.size() - 1 - count] != after[after.size() - 1 - count]) break;
  }
  return count;
}

void add_range(std::vector<DiffLine>& out, DiffMark mark, const Lines& lines, std::size_t from, std::size_t to) {
  for (std::size_t index = from; index < to; ++index) out.push_back({mark, lines[index]});
}

void cap(std::vector<DiffLine>& lines) {
  if (lines.size() <= kMaxDiffLines) return;
  const std::size_t left_out = lines.size() - kMaxDiffLines;
  lines.resize(kMaxDiffLines);
  lines.push_back({DiffMark::Skipped, std::to_string(left_out)});
}

}  // namespace

std::vector<DiffLine> diff_lines(const std::string& before, const std::string& after) {
  const Lines old_lines = split_lines(before);
  const Lines new_lines = split_lines(after);
  const std::size_t start = common_start(old_lines, new_lines);
  const std::size_t end = common_end(old_lines, new_lines, start);
  std::vector<DiffLine> out;
  add_range(out, DiffMark::Kept, old_lines, start - std::min(start, kContextLines), start);
  add_range(out, DiffMark::Removed, old_lines, start, old_lines.size() - end);
  add_range(out, DiffMark::Added, new_lines, start, new_lines.size() - end);
  const std::size_t after_change = old_lines.size() - end;
  add_range(out, DiffMark::Kept, old_lines, after_change, after_change + std::min(end, kContextLines));
  cap(out);
  return out;
}

}  // namespace redgem::agent
