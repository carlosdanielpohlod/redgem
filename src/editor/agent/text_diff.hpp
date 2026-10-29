#pragma once

// A file's change as the lines a tool call card shows: those kept around it, those taken out (-)
// and those put in (+).

#include <cstddef>
#include <string>
#include <vector>

namespace redgem::agent {

// Lines kept unchanged on each side of a change, to show where it is.
inline constexpr std::size_t kContextLines = 2;
// The most lines a diff shows; past them a last line says how many more there were.
inline constexpr std::size_t kMaxDiffLines = 400;

enum class DiffMark { Kept, Removed, Added, Skipped };
struct DiffLine {
  DiffMark mark = DiffMark::Kept;
  std::string text;  // for Skipped, how many lines were left out
};

// The lines of `before` and `after` past what both start and end with (a few of those kept for
// context): what was there as removed, what is there as added.
std::vector<DiffLine> diff_lines(const std::string& before, const std::string& after);

}  // namespace redgem::agent
