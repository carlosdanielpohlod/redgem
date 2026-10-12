#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace redgem::net {

// The longest message a peer may send. Longer ones mean a confused or hostile peer.
inline constexpr std::size_t kMaxFrameBytes = 1024 * 1024;

// Messages on a stream of bytes: each one travels as its length (4 bytes, big-endian) and then
// its text. `frame` wraps a message for sending; a FrameBuffer cuts what arrives, in whatever
// pieces the network delivers it, back into messages.
std::vector<std::uint8_t> frame(const std::string& message);

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

 private:
  // Forgets the bytes of the messages already taken.
  void compact();

  std::vector<std::uint8_t> bytes_;
  // Where the next message starts. Messages taken are skipped, not erased one by one: erasing
  // moves everything after them, which a peer sending many tiny messages would make costly.
  std::size_t taken_ = 0;
  bool broken_ = false;
};

}  // namespace redgem::net
