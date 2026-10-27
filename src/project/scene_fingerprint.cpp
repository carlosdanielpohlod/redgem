#include "project/scene_fingerprint.hpp"

#include <mbedtls/sha256.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace redgem::tools::snapshot {
namespace {

constexpr std::size_t kDigestBytes = 32;
constexpr std::size_t kHashBufferBytes = 4096;
constexpr int kHexDigitsPerByte = 2;
constexpr int kBitsPerByte = 8;
constexpr std::uint64_t kByteMask = 0xFF;

void checked(int result) {
  if (result != 0) throw std::runtime_error("cannot calculate the scene content fingerprint");
}

std::string hexadecimal(const std::array<unsigned char, kDigestBytes>& bytes) {
  std::ostringstream text;
  text << std::hex << std::setfill('0');
  for (const auto byte : bytes) text << std::setw(kHexDigitsPerByte) << static_cast<unsigned int>(byte);
  return text.str();
}

class Hash {
 public:
  Hash() {
    mbedtls_sha256_init(&context_);
    const int result = mbedtls_sha256_starts(&context_, 0);
    if (result == 0) return;
    mbedtls_sha256_free(&context_);
    checked(result);
  }
  ~Hash() { mbedtls_sha256_free(&context_); }
  Hash(const Hash&) = delete;
  Hash& operator=(const Hash&) = delete;

  void integer(std::uint64_t value, std::size_t bytes = sizeof(std::uint64_t)) {
    for (std::size_t i = 0; i < bytes; ++i) {
      buffer_[filled_++] = static_cast<unsigned char>((value >> (i * kBitsPerByte)) & kByteMask);
      if (filled_ == buffer_.size()) flush();
    }
  }

  void scalar(float value) {
    const float canonical = value == 0.0f ? 0.0f : value;
    std::uint32_t bits = 0;
    std::memcpy(&bits, &canonical, sizeof(bits));
    integer(bits, sizeof(bits));
  }
  template <typename T>
  void scalar(T value) {
    integer(static_cast<std::uint64_t>(value), sizeof(T));
  }

  template <typename Range>
  void range(const Range& values) {
    integer(values.size());
    for (const auto value : values) scalar(value);
  }

  void bytes(const std::uint8_t* data, std::size_t size) {
    while (size > 0) {
      const std::size_t count = std::min(size, buffer_.size() - filled_);
      std::memcpy(buffer_.data() + filled_, data, count);
      data += count;
      size -= count;
      filled_ += count;
      if (filled_ == buffer_.size()) flush();
    }
  }

  std::string finish() {
    flush();
    std::array<unsigned char, kDigestBytes> digest{};
    checked(mbedtls_sha256_finish(&context_, digest.data()));
    return hexadecimal(digest);
  }

 private:
  void flush() {
    if (filled_ != 0) checked(mbedtls_sha256_update(&context_, buffer_.data(), filled_));
    filled_ = 0;
  }

  mbedtls_sha256_context context_;
  std::array<unsigned char, kHashBufferBytes> buffer_{};
  std::size_t filled_ = 0;
};

void add_primitive(Hash& hash, const PrimitiveDesc& primitive) {
  hash.range(primitive.positions);
  hash.range(primitive.normals);
  hash.range(primitive.texcoords);
  hash.range(primitive.colors);
  hash.range(primitive.indices);
  hash.range(primitive.joints);
  hash.range(primitive.weights);
}

}  // namespace

std::string mesh_key(const MeshDesc& mesh) {
  Hash hash;
  hash.integer(mesh.primitives.size());
  for (const auto& primitive : mesh.primitives) add_primitive(hash, primitive);
  return hash.finish();
}

std::string image_key(const ImageData& image) {
  Hash hash;
  hash.integer(image.encoded.size());
  hash.bytes(image.encoded.data(), image.encoded.size());
  return hash.finish();
}

std::string bind_key(const SkinDesc& skin) {
  Hash hash;
  hash.integer(skin.inverse_bind.size());
  for (const auto& matrix : skin.inverse_bind) hash.range(matrix);
  return hash.finish();
}

std::string channel_key(const AnimationChannel& channel) {
  Hash hash;
  hash.range(channel.times);
  hash.range(channel.values);
  return hash.finish();
}

}  // namespace redgem::tools::snapshot
