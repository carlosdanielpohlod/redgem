#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace redgem::net {

// The longest message a peer may send. Longer ones mean a confused or hostile peer.
inline constexpr std::size_t kMaxFrameBytes = 1024 * 1024;
// What a buffer keeps once a large message has gone: it gives memory back down to this size, and
// only when what it holds fills less than 1/kShrinkRatio of what it took (so a connection that
// keeps sending large messages does not shrink and grow again on every one).
inline constexpr std::size_t kRetainedFrameBytes = 64 * 1024;
inline constexpr std::size_t kShrinkRatio = 4;

// Messages on a stream of bytes: each one travels as its length (4 bytes, big-endian) and then
// its bytes. `frame` wraps a message for sending; a FrameBuffer cuts what arrives, in whatever
// pieces the network delivers it, back into messages.
std::vector<std::uint8_t> frame(const std::vector<std::uint8_t>& message);

class FrameBuffer {
 public:
  void append(const std::uint8_t* bytes, std::size_t count);
  // The next whole message, if all of it has arrived.
  std::optional<std::string> next();
  // Whether next() has something to give (or to refuse) without anything more arriving.
  bool message_waiting() const;
  // True once a message longer than kMaxFrameBytes was announced: nothing more can be trusted.
  bool broken() const { return broken_; }
  // The bytes that arrived and are not part of a message taken yet.
  std::size_t buffered() const { return bytes_.size() - taken_; }
  // The memory it holds for bytes, used or not.
  std::size_t capacity() const { return bytes_.capacity(); }

 private:
  // Whether the length of the next message can be read: it arrived, and what arrives is still trusted.
  bool length_arrived() const;
  // Forgets the bytes of the messages already taken, and gives memory back once they are few.
  void compact();
  // Whether it holds far more memory than its bytes use (see kShrinkRatio).
  bool oversized() const;
  // Keeps its bytes in kRetainedFrameBytes of memory (or as many as they need).
  void shrink();

  std::vector<std::uint8_t> bytes_;
  // Where the next message starts. Messages taken are skipped, not erased one by one: erasing
  // moves everything after them, which a peer sending many tiny messages would make costly.
  std::size_t taken_ = 0;
  bool broken_ = false;
};

}  // namespace redgem::net
