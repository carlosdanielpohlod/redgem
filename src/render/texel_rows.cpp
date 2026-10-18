#include "render/texel_rows.hpp"

#include <algorithm>

#include <raymath.h>

#include <cstring>

namespace redgem::render_math {
namespace {

constexpr int kGrowth = 2;  // rows double as they grow
constexpr std::size_t kFloatsPerRow = static_cast<std::size_t>(kTexelsPerRow) * kFloatsPerTexel;

// The rows that hold `texels`: a power of two, so that a texture growing a little at a time is
// made again only a few times.
int rows_for(std::size_t texels) {
  const std::size_t needed = (texels + kTexelsPerRow - 1) / kTexelsPerRow;
  int rows = 1;
  while (static_cast<std::size_t>(rows) < needed) rows *= kGrowth;
  return rows;
}

// Whether `row` comes right after the last run.
bool extends(const std::vector<RowRun>& runs, int row) {
  return !runs.empty() && runs.back().first + runs.back().count == row;
}

}  // namespace

TexelPlace texel_place(std::size_t texel) {
  return {static_cast<int>(texel % kTexelsPerRow), static_cast<int>(texel / kTexelsPerRow)};
}

std::vector<TexelSpan> texel_spans(std::size_t first, std::size_t count) {
  std::vector<TexelSpan> spans;
  for (std::size_t at = first, end = first + count; at < end;) {
    const TexelPlace place = texel_place(at);
    const std::size_t room = static_cast<std::size_t>(kTexelsPerRow - place.x);
    const std::size_t taken = std::min(room, end - at);
    spans.push_back({place, static_cast<int>(taken)});
    at += taken;
  }
  return spans;
}

bool TexelRows::write(std::size_t first, const float* values, std::size_t count) {
  const std::size_t end = first + count;
  if (end > kMaxTexels) return false;
  if (end > static_cast<std::size_t>(rows()) * kTexelsPerRow) grow_to(rows_for(end));
  float* place = floats_.data() + first * kFloatsPerTexel;
  const std::size_t bytes = count * kFloatsPerTexel * sizeof(float);
  if (std::memcmp(place, values, bytes) == 0) return true;  // most of a scene stands still
  std::memcpy(place, values, bytes);
  mark(first, count);
  return true;
}

bool TexelRows::write_matrix(std::size_t first, const Matrix& matrix) {
  return write(first, MatrixToFloatV(matrix).v, kTexelsPerMatrix);
}

const float* TexelRows::row(int index) const {
  return floats_.data() + static_cast<std::size_t>(index) * kFloatsPerRow;
}

void TexelRows::take_changed(std::vector<RowRun>& runs) {
  runs.clear();
  for (int row = 0; row < rows(); ++row) {
    if (!changed_[static_cast<std::size_t>(row)]) continue;
    changed_[static_cast<std::size_t>(row)] = false;
    if (extends(runs, row)) {
      ++runs.back().count;
    } else {
      runs.push_back({row, 1});
    }
  }
}

void TexelRows::grow_to(int rows) {
  floats_.resize(static_cast<std::size_t>(rows) * kFloatsPerRow, 0.0f);
  changed_.resize(static_cast<std::size_t>(rows), false);
  grew_ = true;
}

void TexelRows::mark(std::size_t first, std::size_t count) {
  const std::size_t last = (first + count - 1) / kTexelsPerRow;
  for (std::size_t row = first / kTexelsPerRow; row <= last; ++row) changed_[row] = true;
}

}  // namespace redgem::render_math
