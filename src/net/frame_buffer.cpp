#include "net/frame_buffer.hpp"

namespace redgem::net {

namespace {

constexpr std::size_t kLengthBytes = 4;
constexpr int kBitsPerByte = 8;
constexpr std::uint32_t kByteMask = 0xFF;

// The length the first bytes of a stream announce.
std::size_t announced_length(const std::vector<std::uint8_t>& bytes) {
  std::size_t length = 0;
  for (std::size_t i = 0; i < kLengthBytes; ++i) length = (length << kBitsPerByte) | bytes[i];
  return length;
}

// Takes the first message, `length` long, off the front of the stream.
std::string take_message(std::vector<std::uint8_t>& bytes, std::size_t length) {
  const auto first = bytes.begin() + static_cast<std::ptrdiff_t>(kLengthBytes);
  const auto last = first + static_cast<std::ptrdiff_t>(length);
  std::string message(first, last);
  bytes.erase(bytes.begin(), last);
  return message;
}

}  // namespace

std::vector<std::uint8_t> frame(const std::string& message) {
  std::vector<std::uint8_t> out;
  out.reserve(kLengthBytes + message.size());
  const auto length = static_cast<std::uint32_t>(message.size());
  for (std::size_t i = 0; i < kLengthBytes; ++i) {
    const int shift = static_cast<int>(kLengthBytes - 1 - i) * kBitsPerByte;
    out.push_back(static_cast<std::uint8_t>((length >> shift) & kByteMask));
  }
  out.insert(out.end(), message.begin(), message.end());
  return out;
}

void FrameBuffer::append(const std::uint8_t* bytes, std::size_t count) {
  if (!broken_) bytes_.insert(bytes_.end(), bytes, bytes + count);
}

std::optional<std::string> FrameBuffer::next() {
  if (broken_ || bytes_.size() < kLengthBytes) return std::nullopt;
  const std::size_t length = announced_length(bytes_);
  if (length > kMaxFrameBytes) {
    broken_ = true;
    bytes_.clear();
    return std::nullopt;
  }
  if (bytes_.size() < kLengthBytes + length) return std::nullopt;
  return take_message(bytes_, length);
}

}  // namespace redgem::net
