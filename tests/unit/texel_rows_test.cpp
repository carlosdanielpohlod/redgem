#include <doctest/doctest.h>

#include <array>
#include <vector>

#include "render/texel_rows.hpp"

#include <raymath.h>  // after raylib.h (through texel_rows.hpp): it relies on raylib's own types

using namespace redgem::render_math;

namespace {

constexpr std::size_t kOneTexel = 1;
constexpr std::size_t kSecondRowStart = kTexelsPerRow;
constexpr std::size_t kFifthRowStart = 4 * static_cast<std::size_t>(kTexelsPerRow);
constexpr std::size_t kLastTexelOfFirstRow = kTexelsPerRow - 1;
constexpr std::size_t kTwoTexels = 2;
constexpr int kRowsForFiveRows = 8;  // rows come in powers of two
constexpr std::array<float, kFloatsPerTexel> kRed{1.0f, 0.0f, 0.0f, 1.0f};
constexpr std::array<float, 2 * kFloatsPerTexel> kRedAndGreen{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
constexpr float kMoveX = 3.0f;
constexpr float kMoveY = -2.0f;
constexpr float kMoveZ = 7.5f;
constexpr int kTranslationColumn = 3;

std::vector<RowRun> changed(TexelRows& rows) {
  std::vector<RowRun> runs;
  rows.take_changed(runs);
  return runs;
}

bool same_runs(const std::vector<RowRun>& runs, const std::vector<RowRun>& expected) {
  if (runs.size() != expected.size()) return false;
  for (std::size_t i = 0; i < runs.size(); ++i) {
    if (runs[i].first != expected[i].first || runs[i].count != expected[i].count) return false;
  }
  return true;
}

const float* texel(const TexelRows& rows, std::size_t index) {
  const TexelPlace place = texel_place(index);
  return rows.row(place.y) + static_cast<std::size_t>(place.x) * kFloatsPerTexel;
}

}  // namespace

TEST_CASE("a texel is found by its row and its place along it") {
  CHECK(texel_place(0).x == 0);
  CHECK(texel_place(kLastTexelOfFirstRow).x == kTexelsPerRow - 1);
  CHECK(texel_place(kLastTexelOfFirstRow).y == 0);
  CHECK(texel_place(kSecondRowStart).x == 0);
  CHECK(texel_place(kSecondRowStart).y == 1);
}

TEST_CASE("the rows grow to hold what is written, in powers of two, and say they grew") {
  TexelRows rows;
  CHECK(rows.rows() == 0);
  REQUIRE(rows.write(kFifthRowStart, kRed.data(), kOneTexel));
  CHECK(rows.rows() == kRowsForFiveRows);
  CHECK(rows.grew());
  rows.taken_whole();
  CHECK_FALSE(rows.grew());
}

TEST_CASE("nothing is written past what the texture can hold") {
  TexelRows rows;
  CHECK_FALSE(rows.write(kMaxTexels, kRed.data(), kOneTexel));
  CHECK_FALSE(rows.write(kMaxTexels - 1, kRedAndGreen.data(), kTwoTexels));  // the second one would not fit
  CHECK(rows.rows() == 0);
}

TEST_CASE("a write that changes texels marks their rows, runs of rows together, once") {
  TexelRows rows;
  REQUIRE(rows.write(kFifthRowStart, kRed.data(), kOneTexel));
  rows.taken_whole();
  CHECK(changed(rows).size() == 1);                                            // the first write
  REQUIRE(rows.write(kLastTexelOfFirstRow, kRedAndGreen.data(), kTwoTexels));  // rows 0 and 1
  REQUIRE(rows.write(kFifthRowStart, kRedAndGreen.data(), kTwoTexels));        // row 4
  CHECK(same_runs(changed(rows), {{0, 2}, {4, 1}}));
  CHECK(changed(rows).empty());
}

TEST_CASE("writing what a texel already holds changes no row") {
  TexelRows rows;
  REQUIRE(rows.write(0, kRed.data(), kOneTexel));
  changed(rows);
  REQUIRE(rows.write(0, kRed.data(), kOneTexel));
  CHECK(changed(rows).empty());
  CHECK(texel(rows, 0)[0] == kRed[0]);
}

TEST_CASE("a matrix goes in as four texels, one column each, as GLSL builds a mat4") {
  TexelRows rows;
  const Matrix moved = MatrixTranslate(kMoveX, kMoveY, kMoveZ);
  REQUIRE(rows.write_matrix(kSecondRowStart, moved));
  const float* translation = texel(rows, kSecondRowStart + kTranslationColumn);
  CHECK(translation[0] == kMoveX);
  CHECK(translation[1] == kMoveY);
  CHECK(translation[2] == kMoveZ);
  CHECK(translation[3] == 1.0f);
  CHECK(texel(rows, kSecondRowStart)[0] == 1.0f);  // the first column: the x axis
}

TEST_CASE("texels from anywhere are cut into spans of one row each") {
  const auto spans = texel_spans(kTexelsPerRow - 2, 5);
  REQUIRE(spans.size() == 2);
  CHECK(spans[0].first.x == kTexelsPerRow - 2);
  CHECK(spans[0].first.y == 0);
  CHECK(spans[0].count == 2);
  CHECK(spans[1].first.x == 0);
  CHECK(spans[1].first.y == 1);
  CHECK(spans[1].count == 3);
  CHECK(texel_spans(7, 0).empty());
}
