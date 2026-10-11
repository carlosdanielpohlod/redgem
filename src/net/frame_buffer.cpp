#include "net/frame_buffer.hpp"

namespace redgem::net {

namespace {

constexpr std::size_t kLengthBytes = 4;
constexpr int kBitsPerByte = 8;
constexpr std::uint32_t kByteMask = 0xFF;

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
  std::size_t length = 0;
  for (std::size_t i = 0; i < kLengthBytes; ++i) length = (length << kBitsPerByte) | bytes_[i];
  if (length > kMaxFrameBytes) {
    broken_ = true;
    bytes_.clear();
    return std::nullopt;
  }
  if (bytes_.size() < kLengthBytes + length) return std::nullopt;
  const auto first = bytes_.begin() + static_cast<std::ptrdiff_t>(kLengthBytes);
  std::string message(first, first + static_cast<std::ptrdiff_t>(length));
  bytes_.erase(bytes_.begin(), first + static_cast<std::ptrdiff_t>(length));
  return message;
}

}  // namespace redgem::net
