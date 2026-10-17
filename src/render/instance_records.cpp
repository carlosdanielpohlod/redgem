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
constexpr int kCopyAt = kCopyTexel * kFloatsPerTexel;
constexpr int kFourthFloat = 3;
constexpr int kSwayAt = kNormalTexel * kFloatsPerTexel + kFourthFloat;  // the normal matrix's first column has room
constexpr int kRed = 0;
constexpr int kGreen = 1;
constexpr int kBlue = 2;
constexpr int kBone = 3;
constexpr int kFrom = 0;  // of the copy texel
constexpr int kTo = 1;
constexpr int kFade = 2;
constexpr int kUpward = 3;

using Record = std::array<float, kRecordFloats>;

void put_look(Record& record, const InstanceLook& look) {
  record[kLookAt + kRed] = look.tint.x;
  record[kLookAt + kGreen] = look.tint.y;
  record[kLookAt + kBlue] = look.tint.z;
  record[kLookAt + kBone] = look.first_bone;
  record[kCopyAt + kFrom] = look.visible_from;
  record[kCopyAt + kTo] = look.visible_to;
  record[kCopyAt + kFade] = look.fade;
  record[kCopyAt + kUpward] = look.upward;
}

}  // namespace

bool write_instance(TexelRows& texels, std::size_t number, const Matrix& model, const InstanceLook& look) {
  Record record{};
  const float16 place = MatrixToFloatV(model);
  // What turns normals: the inverse of the model matrix, transposed (right however it is scaled).
  const float16 normals = MatrixToFloatV(MatrixTranspose(MatrixInvert(model)));
  std::copy(place.v, place.v + kMatrixFloats, record.begin());
  std::copy(normals.v, normals.v + kNormalColumnsFloats, record.begin() + kNormalTexel * kFloatsPerTexel);
  put_look(record, look);
  record[kSwayAt] = look.sway;
  return texels.write(record_texel(number), record.data(), kTexelsPerInstance);
}

}  // namespace redgem::render_math
