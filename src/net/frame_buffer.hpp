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
  // True once a message longer than kMaxFrameBytes was announced: nothing more can be trusted.
  bool broken() const { return broken_; }

 private:
  std::vector<std::uint8_t> bytes_;
  bool broken_ = false;
};

}  // namespace redgem::net
