#include "net/pose_codec.hpp"

#include <cstring>
#include <utility>

#include "net/wire_codec.hpp"

namespace redgem::net {

namespace {

// The flags: which parts follow.
constexpr std::uint8_t kAbsolutePosition = 1;
constexpr std::uint8_t kMovedPosition = 2;
constexpr std::uint8_t kRotation = 4;
constexpr std::uint8_t kKnownFlags = kAbsolutePosition | kMovedPosition | kRotation;
// The sizes of the parts, in bytes.
constexpr std::size_t kHeadBytes = 2;  // flags and key size
constexpr std::size_t kFloatBytes = 4;
constexpr std::size_t kStepBytes = 2;
constexpr std::size_t kRotationBytes = 4;
constexpr std::size_t kFlagsAt = 0;
constexpr std::size_t kKeySizeAt = 1;
constexpr int kBitsPerByte = 8;
constexpr std::uint32_t kByteMask = 0xFF;

bool has(std::uint8_t flags, std::uint8_t flag) { return (flags & flag) != 0; }

class Writer {
 public:
  void byte(std::uint8_t value) { bytes_.push_back(value); }
  // The `size` lowest bytes of `value`, lowest first.
  void word(std::uint32_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) byte(static_cast<std::uint8_t>((value >> (i * kBitsPerByte)) & kByteMask));
  }
  void text(const std::string& value) { bytes_.insert(bytes_.end(), value.begin(), value.end()); }
  void vector(Vec3 value) {
    for (const float axis : {value.x, value.y, value.z}) {
      std::uint32_t bits = 0;
      std::memcpy(&bits, &axis, sizeof(bits));
      word(bits, kFloatBytes);
    }
  }
  void steps(const PoseSteps& value) {
    for (const std::int16_t axis : value) word(static_cast<std::uint16_t>(axis), kStepBytes);
  }
  std::vector<std::uint8_t> take() { return std::move(bytes_); }

 private:
  std::vector<std::uint8_t> bytes_;
};

// Reads bytes known to be there (decoded checks the size first).
class Reader {
 public:
  Reader(const std::vector<std::uint8_t>& bytes, std::size_t at) : bytes_(bytes), at_(at) {}

  std::uint32_t word(std::size_t size) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < size; ++i) value |= static_cast<std::uint32_t>(bytes_[at_ + i]) << (i * kBitsPerByte);
    at_ += size;
    return value;
  }
  std::string text(std::size_t size) {
    const auto first = bytes_.begin() + static_cast<std::ptrdiff_t>(at_);
    at_ += size;
    return {first, first + static_cast<std::ptrdiff_t>(size)};
  }
  float number() {
    const std::uint32_t bits = word(kFloatBytes);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }
  Vec3 vector() {
    const float x = number();
    const float y = number();
    return {x, y, number()};
  }
  std::int16_t step() { return static_cast<std::int16_t>(static_cast<std::uint16_t>(word(kStepBytes))); }
  PoseSteps steps() {
    const std::int16_t x = step();
    const std::int16_t y = step();
    return {x, y, step()};
  }

 private:
  const std::vector<std::uint8_t>& bytes_;
  std::size_t at_;
};

std::uint8_t flags_of(const PoseUpdate& update) {
  std::uint8_t flags = 0;
  if (update.position) flags |= kAbsolutePosition;
  if (update.moved) flags |= kMovedPosition;
  if (update.rotation) flags |= kRotation;
  return flags;
}

// Whether the flags and key size could be a pose's: flags it knows, never both kinds of position,
// and a key no longer than a key may be.
bool sensible(std::uint8_t flags, std::size_t key_size) {
  const bool both_positions = has(flags, kAbsolutePosition) && has(flags, kMovedPosition);
  return (flags & ~kKnownFlags) == 0 && !both_positions && key_size <= kMaxPoseKeyBytes;
}

std::size_t size_of(std::uint8_t flags, std::size_t key_size) {
  return kHeadBytes + key_size + (has(flags, kAbsolutePosition) ? kPoseAxes * kFloatBytes : 0) +
         (has(flags, kMovedPosition) ? kPoseAxes * kStepBytes : 0) + (has(flags, kRotation) ? kRotationBytes : 0);
}

// Whether the bytes are laid out as a pose: their head makes sense, and says how many there are.
bool laid_out_as_a_pose(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < kHeadBytes) return false;
  const std::uint8_t flags = bytes[kFlagsAt];
  const std::size_t key_size = bytes[kKeySizeAt];
  return sensible(flags, key_size) && bytes.size() == size_of(flags, key_size);
}

void read_parts(Reader& reader, std::uint8_t flags, PoseUpdate& update) {
  if (has(flags, kAbsolutePosition)) update.position = reader.vector();
  if (has(flags, kMovedPosition)) update.moved = reader.steps();
  if (has(flags, kRotation)) update.rotation = reader.word(kRotationBytes);
}

}  // namespace

std::vector<std::uint8_t> encoded(const PoseUpdate& update) {
  Writer writer;
  writer.byte(flags_of(update));
  writer.byte(static_cast<std::uint8_t>(update.key.size()));
  writer.text(update.key);
  if (update.position) writer.vector(*update.position);
  if (update.moved) writer.steps(*update.moved);
  if (update.rotation) writer.word(*update.rotation, kRotationBytes);
  return writer.take();
}

std::optional<PoseUpdate> decoded(const std::vector<std::uint8_t>& bytes) {
  if (!laid_out_as_a_pose(bytes)) return std::nullopt;
  Reader reader(bytes, kHeadBytes);
  PoseUpdate update;
  update.key = reader.text(bytes[kKeySizeAt]);
  if (!wire::valid_utf8(update.key)) return std::nullopt;
  read_parts(reader, bytes[kFlagsAt], update);
  return update;
}

}  // namespace redgem::net
