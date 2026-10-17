#include "render/instance_records.hpp"

#include <raymath.h>

#include <algorithm>
#include <array>

namespace redgem::render_math {
namespace {

constexpr int kRecordFloats = kTexelsPerInstance * kFloatsPerTexel;
constexpr int kMatrixFloats = kTexelsPerMatrix * kFloatsPerTexel;
constexpr int kNormalColumnsFloats = 3 * kFloatsPerTexel;  // the normal matrix's first three columns
constexpr int kLookAt = kLookTexel * kFloatsPerTexel;
constexpr int kRed = 0;
constexpr int kGreen = 1;
constexpr int kBlue = 2;
constexpr int kBone = 3;

}  // namespace

bool write_instance(TexelRows& texels, std::size_t number, const Matrix& model, const InstanceLook& look) {
  std::array<float, kRecordFloats> record{};
  const float16 place = MatrixToFloatV(model);
  // What turns normals: the inverse of the model matrix, transposed (right however it is scaled).
  const float16 normals = MatrixToFloatV(MatrixTranspose(MatrixInvert(model)));
  std::copy(place.v, place.v + kMatrixFloats, record.begin());
  std::copy(normals.v, normals.v + kNormalColumnsFloats, record.begin() + kNormalTexel * kFloatsPerTexel);
  record[kLookAt + kRed] = look.tint.x;
  record[kLookAt + kGreen] = look.tint.y;
  record[kLookAt + kBlue] = look.tint.z;
  record[kLookAt + kBone] = look.first_bone;
  return texels.write(record_texel(number), record.data(), kTexelsPerInstance);
}

}  // namespace redgem::render_math
