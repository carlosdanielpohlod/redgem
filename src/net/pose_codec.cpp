#include "net/pose_codec.hpp"

#include "net/wire_codec.hpp"

namespace redgem::net {

namespace {

// The flags: which parts follow.
constexpr std::uint8_t kAbsolutePosition = 1;
constexpr std::uint8_t kMovedPosition = 2;
constexpr std::uint8_t kRotation = 4;
constexpr std::uint8_t kAnimation = 8;
constexpr std::uint8_t kKnownFlags = kAbsolutePosition | kMovedPosition | kRotation | kAnimation;
// The sizes of the parts, in bytes.
constexpr std::size_t kHeadBytes = 2;  // flags and key size
constexpr std::size_t kRotationBytes = 4;
constexpr std::size_t kFlagsAt = 0;
constexpr std::size_t kKeySizeAt = 1;

std::uint8_t flags_of(const PoseUpdate& update) {
  std::uint8_t flags = 0;
  if (update.position) flags |= kAbsolutePosition;
  if (update.moved) flags |= kMovedPosition;
  if (update.rotation) flags |= kRotation;
  if (update.animation) flags |= kAnimation;
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

// Whether there are as many bytes as the flags and the key size say: exactly, or more for an
// animation (whose own size it says itself, see animation_codec.hpp).
bool sized_as_said(std::size_t size, std::uint8_t flags, std::size_t key_size) {
  return has(flags, kAnimation) ? size > size_of(flags, key_size) : size == size_of(flags, key_size);
}

// Whether the bytes are laid out as a pose: their head makes sense, and says how many there are.
bool laid_out_as_a_pose(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < kHeadBytes) return false;
  const std::uint8_t flags = bytes[kFlagsAt];
  const std::size_t key_size = bytes[kKeySizeAt];
  return sensible(flags, key_size) && sized_as_said(bytes.size(), flags, key_size);
}

// The parts after the key; false when an animation the flags announce is not one.
bool read_parts(PoseReader& reader, std::uint8_t flags, PoseUpdate& update) {
  if (has(flags, kAbsolutePosition)) update.position = reader.vector();
  if (has(flags, kMovedPosition)) update.moved = reader.steps();
  if (has(flags, kRotation)) update.rotation = reader.word(kRotationBytes);
  if (!has(flags, kAnimation)) return true;
  update.animation = read_animation(reader);
  return update.animation.has_value();
}

}  // namespace

std::vector<std::uint8_t> encoded(const PoseUpdate& update) {
  PoseWriter writer;
  writer.byte(flags_of(update));
  writer.byte(static_cast<std::uint8_t>(update.key.size()));
  writer.text(update.key);
  if (update.position) writer.vector(*update.position);
  if (update.moved) writer.steps(*update.moved);
  if (update.rotation) writer.word(*update.rotation, kRotationBytes);
  if (update.animation) write_animation(writer, *update.animation);
  return writer.take();
}

std::optional<PoseUpdate> decoded(const std::vector<std::uint8_t>& bytes) {
  if (!laid_out_as_a_pose(bytes)) return std::nullopt;
  PoseReader reader(bytes, kHeadBytes);
  PoseUpdate update;
  update.key = reader.text(bytes[kKeySizeAt]);
  if (!wire::valid_utf8(update.key)) return std::nullopt;
  if (!read_parts(reader, bytes[kFlagsAt], update)) return std::nullopt;
  return update;
}

}  // namespace redgem::net
