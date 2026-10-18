#pragma once

// What the vertex shader reads of each frame that is not a vertex: where every copy of a mesh is
// and the bones of every skeleton, as matrices in a texture of float texels (a matrix is four
// texels, a column each). Uniforms hold only so many matrices; a texture holds as many as there
// are. This is the copy kept here, row by row, which remembers what changed since it last went to
// the graphics card. No window and no graphics card needed, so it can be tested.

#include <raylib.h>

#include <cstddef>
#include <vector>

namespace redgem::render_math {

inline constexpr int kTexelsPerRow = 2048;  // the texture's width: what every OpenGL ES 3 card holds
inline constexpr int kMaxTexelRows = 2048;  // and its height, at most
inline constexpr int kFloatsPerTexel = 4;   // red, green, blue, alpha
inline constexpr int kTexelsPerMatrix = 4;  // a 4x4 matrix, a column a texel
inline constexpr int kTexelFormat = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;  // four floats a texel
inline constexpr int kTexelLevels = 1;  // no smaller copies: texels are fetched one by one, never filtered
inline constexpr std::size_t kMaxTexels = static_cast<std::size_t>(kTexelsPerRow) * kMaxTexelRows;
// Where a skeleton's first bone is, for what has no skeleton (the shader takes texels as floats).
inline constexpr float kNoBones = -1.0f;

// Where a texel is in the texture.
struct TexelPlace {
  int x = 0;
  int y = 0;
};
TexelPlace texel_place(std::size_t texel);

// Texels next to each other on one row: `count` of them from `first` on.
struct TexelSpan {
  TexelPlace first;
  int count = 0;
};
// The texels from `first` on, `count` of them, as spans of a row each.
std::vector<TexelSpan> texel_spans(std::size_t first, std::size_t count);

// Rows next to each other.
struct RowRun {
  int first = 0;
  int count = 0;
};

class TexelRows {
 public:
  // Writes `count` texels (four floats each) from texel `first` on, growing the rows to hold them.
  // False, with nothing written, past kMaxTexels. A row whose texels change counts as changed.
  bool write(std::size_t first, const float* values, std::size_t count);
  // A matrix as four texels, a column each (the order GLSL builds a mat4 in).
  bool write_matrix(std::size_t first, const Matrix& matrix);

  // How many rows there are: whole powers of two, so that growing is seldom.
  int rows() const { return static_cast<int>(changed_.size()); }
  const float* row(int index) const;
  // Runs of changed rows, in order, into `runs` (emptied first); they then count as unchanged.
  void take_changed(std::vector<RowRun>& runs);
  // Whether the rows grew since the texture last took them all (it has to be made again).
  bool grew() const { return grew_; }
  void taken_whole() { grew_ = false; }

 private:
  void grow_to(int rows);
  void mark(std::size_t first, std::size_t count);

  std::vector<float> floats_;
  std::vector<bool> changed_;  // per row
  bool grew_ = false;
};

}  // namespace redgem::render_math
