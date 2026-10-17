#include "net/animation_codec.hpp"

#include "net/wire_codec.hpp"

namespace redgem::net {
namespace {

// A clip's flags.
constexpr std::uint8_t kLoops = 1;
constexpr std::uint8_t kPlays = 2;
constexpr std::uint8_t kKnownClipFlags = kLoops | kPlays;
constexpr std::size_t kNoClip = 0;
constexpr std::size_t kClipNumbersBytes = 2 * kFloatBytes + kByte;  // time, speed and flags
constexpr std::size_t kHeadBytes = 2 * kFloatBytes;                 // fade and layer weight

// Whether what was read is a clip: a name in UTF-8, and flags a clip has.
bool sound_clip(const std::string& name, std::uint32_t flags) {
  return wire::valid_utf8(name) && (flags & ~kKnownClipFlags) == 0;
}

// Whether a clip, if any, has a name a clip carries: an empty one would read as no clip at all.
bool fits(const std::optional<ClipPart>& clip) {
  return !clip || (!clip->clip.empty() && clip->clip.size() <= kMaxClipNameBytes);
}

void write_clip(PoseWriter& writer, const std::optional<ClipPart>& clip) {
  if (!clip) {
    writer.byte(kNoClip);
    return;
  }
  writer.byte(static_cast<std::uint8_t>(clip->clip.size()));
  writer.text(clip->clip);
  writer.number(clip->time);
  writer.number(clip->speed);
  writer.byte(static_cast<std::uint8_t>((clip->loop ? kLoops : 0) | (clip->playing ? kPlays : 0)));
}

// Whether what is left holds a clip of a name `size` bytes long, a name a clip may have.
bool holds_clip(const PoseReader& reader, std::size_t size) {
  return size <= kMaxClipNameBytes && reader.left() >= size + kClipNumbersBytes;
}

// A clip of a name `size` bytes long (that much is there); nothing when its name is not UTF-8 or
// its flags are not a clip's.
std::optional<ClipPart> clip_of(PoseReader& reader, std::size_t size) {
  ClipPart clip;
  clip.clip = reader.text(size);
  clip.time = reader.number();
  clip.speed = reader.number();
  const std::uint32_t flags = reader.word(kByte);
  if (!sound_clip(clip.clip, flags)) return std::nullopt;
  clip.loop = has(flags, kLoops);
  clip.playing = has(flags, kPlays);
  return clip;
}

// Reads a clip, or its absence, into `clip`: false when the bytes are not one.
bool read_clip(PoseReader& reader, std::optional<ClipPart>& clip) {
  if (reader.left() < kByte) return false;
  const std::size_t size = reader.word(kByte);
  if (size == kNoClip) return true;
  if (!holds_clip(reader, size)) return false;
  clip = clip_of(reader, size);
  return clip.has_value();
}

}  // namespace

bool carriable(const AnimationPart& part) { return fits(part.own) && fits(part.layer); }

void write_animation(PoseWriter& writer, const AnimationPart& part) {
  writer.number(part.fade);
  writer.number(part.layer_weight);
  write_clip(writer, part.own);
  write_clip(writer, part.layer);
}

std::optional<AnimationPart> read_animation(PoseReader& reader) {
  if (reader.left() < kHeadBytes) return std::nullopt;
  AnimationPart part;
  part.fade = reader.number();
  part.layer_weight = reader.number();
  if (!read_clip(reader, part.own)) return std::nullopt;
  if (!read_clip(reader, part.layer)) return std::nullopt;
  if (reader.left() != 0) return std::nullopt;
  return part;
}

}  // namespace redgem::net
