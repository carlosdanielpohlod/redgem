#include "net/frame_buffer.hpp"

namespace redgem::net {

namespace {

constexpr std::size_t kLengthBytes = 4;
constexpr int kBitsPerByte = 8;
constexpr std::uint32_t kByteMask = 0xFF;

// The length the bytes at `start` announce.
std::size_t announced_length(const std::vector<std::uint8_t>& bytes, std::size_t start) {
  std::size_t length = 0;
  for (std::size_t i = 0; i < kLengthBytes; ++i) length = (length << kBitsPerByte) | bytes[start + i];
  return length;
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

void FrameBuffer::compact() {
  bytes_.erase(bytes_.begin(), bytes_.begin() + static_cast<std::ptrdiff_t>(taken_));
  taken_ = 0;
}

void FrameBuffer::append(const std::uint8_t* bytes, std::size_t count) {
  if (broken_) return;
  compact();  // once for everything taken since the last append, however many messages that was
  bytes_.insert(bytes_.end(), bytes, bytes + count);
}

std::optional<std::string> FrameBuffer::next() {
  if (broken_ || buffered() < kLengthBytes) return std::nullopt;
  const std::size_t length = announced_length(bytes_, taken_);
  if (length > kMaxFrameBytes) {
    broken_ = true;
    bytes_.clear();
    taken_ = 0;
    return std::nullopt;
  }
  if (buffered() < kLengthBytes + length) return std::nullopt;
  const auto first = bytes_.begin() + static_cast<std::ptrdiff_t>(taken_ + kLengthBytes);
  taken_ += kLengthBytes + length;
  return std::string(first, first + static_cast<std::ptrdiff_t>(length));
}

}  // namespace redgem::net
