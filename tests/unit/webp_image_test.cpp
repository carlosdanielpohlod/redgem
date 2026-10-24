#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

#include "core/image_header.hpp"
#include "core/safe_path.hpp"
#include "core/webp_image.hpp"
#include "scene/gltf/gltf_budget.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "test_support.hpp"

using namespace redgem;
using nlohmann::json;

namespace {
constexpr std::uint32_t kSide = 16;
constexpr std::size_t kChannels = 4;
constexpr std::size_t kImageBytes = kSide * kSide * kChannels;
constexpr std::uint8_t kCanary = 0xda;
constexpr std::size_t kLosslessDimensionsAt = 21;
constexpr unsigned kHeightShift = 14;
constexpr unsigned kAlphaShift = 28;
constexpr unsigned kByteBits = 8;
constexpr int kPastSceneBudget = 17;  // 17 RGBA8 images of 8192 squared exceed four GiB

std::vector<std::uint8_t> webp_bytes(const std::string& name) {
  const auto bytes = read_file_up_to(test::fixture("images/" + name + ".webp"), kMaxEncodedImageBytes);
  REQUIRE(bytes.has_value());
  return *bytes;
}
std::vector<std::uint8_t> lossless_of_size(std::uint32_t width, std::uint32_t height) {
  auto encoded = webp_bytes("lossless");
  const auto dimensions = (width - 1) | ((height - 1) << kHeightShift) | (std::uint32_t{1} << kAlphaShift);
  for (std::size_t i = 0; i < sizeof(dimensions); ++i)
    encoded[kLosslessDimensionsAt + i] = static_cast<std::uint8_t>(dimensions >> (kByteBits * i));
  return encoded;
}
struct WebpScene {
  std::filesystem::path folder = std::filesystem::temp_directory_path() / "redgem-webp-scene-test";
  WebpScene() {
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    std::filesystem::copy_file(test::fixture("images/lossless.webp"), folder / "picture.webp");
  }
  ~WebpScene() { std::filesystem::remove_all(folder); }
  LoadResult load(const json& doc) const {
    const auto file = folder / "scene.gltf";
    std::ofstream(file) << doc.dump();
    return load_gltf_scene(file.string());
  }
};
json material_scene() {
  return {{"asset", {{"version", "2.0"}}},
          {"scene", 0},
          {"scenes", json::array({{{"nodes", {0}}}})},
          {"nodes", json::array({{{"name", "Root"}}})},
          {"images", json::array({{{"uri", "picture.webp"}, {"mimeType", "image/webp"}}})},
          {"textures", json::array({{{"source", 0}}})},
          {"materials", json::array({{{"pbrMetallicRoughness", {{"baseColorTexture", {{"index", 0}}}}}}})}};
}
}  // namespace

TEST_CASE("static lossy and lossless WebP dimensions participate in image limits and scene budgets") {
  for (const auto* name : {"lossless", "lossy"}) {
    const auto bytes = webp_bytes(name);
    const auto size = encoded_image_size(bytes.data(), bytes.size());
    REQUIRE(size.has_value());
    CHECK(size->width == kSide);
    CHECK(size->height == kSide);
    CHECK(decodable_image(bytes.data(), bytes.size()));
    std::vector<std::uint8_t> decoded(kImageBytes);
    CHECK(decode_webp_rgba_into(bytes.data(), bytes.size(), decoded.data(), decoded.size(), kSide * kChannels));
    CHECK(gltf_budget::decoded_bytes({ImageData{bytes, ".webp"}}) == kImageBytes);
  }
  const auto oversized = lossless_of_size(kMaxImageSide + 1, kSide);
  REQUIRE(webp_image_size(oversized.data(), oversized.size()).has_value());
  CHECK_FALSE(decodable_image(oversized.data(), oversized.size()));
}

TEST_CASE("lossless WebP preserves RGBA pixels and respects caller buffer bounds") {
  const auto bytes = webp_bytes("lossless");
  std::vector<std::uint8_t> output(kImageBytes + 2, kCanary);
  REQUIRE(decode_webp_rgba_into(bytes.data(), bytes.size(), output.data() + 1, kImageBytes, kSide * kChannels));
  CHECK(output.front() == kCanary);
  CHECK(output.back() == kCanary);
  const std::array<std::uint8_t, kChannels> left{255, 0, 255, 255};
  const std::array<std::uint8_t, kChannels> right{0, 255, 0, 128};
  CHECK(std::equal(left.begin(), left.end(), output.begin() + 1));
  CHECK(std::equal(right.begin(), right.end(), output.begin() + 1 + (kSide - 1) * kChannels));
  CHECK_FALSE(decode_webp_rgba_into(bytes.data(), bytes.size(), output.data() + 1, kImageBytes - 1, kSide * kChannels));
  CHECK_FALSE(decode_webp_rgba_into(bytes.data(), bytes.size(), output.data() + 1, kImageBytes, 1));
  CHECK_FALSE(decode_webp_rgba_into(bytes.data(), bytes.size(), nullptr, kImageBytes, kSide * kChannels));
  CHECK(output.front() == kCanary);
  CHECK(output.back() == kCanary);
}

TEST_CASE("animated truncated and malformed WebP is refused without an output overrun") {
  const auto animated = webp_bytes("animated");
  CHECK_FALSE(encoded_image_size(animated.data(), animated.size()).has_value());
  CHECK_FALSE(decodable_image(animated.data(), animated.size()));
  std::array<std::uint8_t, kImageBytes> output{};
  CHECK_FALSE(decode_webp_rgba_into(animated.data(), animated.size(), output.data(), output.size(), kSide * kChannels));
  auto invalid = webp_bytes("lossless");
  invalid[0] = 0;
  CHECK_FALSE(encoded_image_size(invalid.data(), invalid.size()).has_value());
  invalid.resize(kLosslessDimensionsAt);
  CHECK_FALSE(decode_webp_rgba_into(invalid.data(), invalid.size(), output.data(), output.size(), kSide * kChannels));
  CHECK_FALSE(webp_image_size(nullptr, 0).has_value());
}

TEST_CASE("glTF EXT_texture_webp chooses its source while plain WebP images also load") {
  WebpScene scene;
  auto doc = material_scene();
  SUBCASE("plain image source") {}
  SUBCASE("extension source without a plain fallback") {
    doc["extensionsUsed"] = {"EXT_texture_webp"};
    doc["textures"][0] = {{"extensions", {{"EXT_texture_webp", {{"source", 0}}}}}};
  }
  SUBCASE("extension takes precedence over the plain fallback") {
    doc["images"].push_back(doc["images"][0]);
    doc["textures"][0]["extensions"] = {{"EXT_texture_webp", {{"source", 1}}}};
  }
  const auto loaded = scene.load(doc);
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  REQUIRE(loaded.scene.materials.size() == 1);
  CHECK(loaded.scene.materials[0].base_color_image == static_cast<int>(loaded.scene.images.size()) - 1);
  CHECK(loaded.scene.images.back().file_extension == ".webp");
}

TEST_CASE("WebP headers cannot bypass the aggregate decoded scene memory budget") {
  WebpScene scene;
  const auto large = lossless_of_size(kMaxImageSide, kMaxImageSide);
  std::ofstream file(scene.folder / "picture.webp", std::ios::binary);
  file.write(reinterpret_cast<const char*>(large.data()), static_cast<std::streamsize>(large.size()));
  file.close();
  auto doc = material_scene();
  for (int i = 1; i < kPastSceneBudget; ++i) doc["images"].push_back(doc["images"][0]);
  const auto loaded = scene.load(doc);
  CHECK_FALSE(loaded.ok);
  CHECK_FALSE(loaded.error.empty());
}

TEST_CASE("a checked image carries the sniffed format and dimensions into WebP decoding") {
  const auto bytes = webp_bytes("lossless");
  const auto image = EncodedImage::read(bytes.data(), bytes.size());
  REQUIRE(image.has_value());
  CHECK(image->format() == EncodedImageFormat::Webp);
  CHECK(image->dimensions().width == kSide);
  CHECK(image->byte_count() == bytes.size());
  std::array<std::uint8_t, kImageBytes> output;
  CHECK(decode_webp_rgba_into(*image, output.data(), output.size(), kSide * kChannels));
  for (const auto* extension : {"png", "jpg"}) {
    const auto other = read_file_up_to(test::fixture(std::string("images/sniff.") + extension), kMaxEncodedImageBytes);
    REQUIRE(other.has_value());
    const auto header = EncodedImage::read(other->data(), other->size());
    REQUIRE(header.has_value());
    CHECK(header->format() != EncodedImageFormat::Webp);
    CHECK_FALSE(decode_webp_rgba_into(*header, output.data(), output.size(), kSide * kChannels));
  }
}

TEST_CASE("glTF images with a WebP MIME hint retain sniffable PNG or JPEG payloads") {
  WebpScene scene;
  for (const auto* extension : {"png", "jpg"}) {
    std::filesystem::copy_file(test::fixture(std::string("images/sniff.") + extension), scene.folder / "picture.webp",
                               std::filesystem::copy_options::overwrite_existing);
    const auto loaded = scene.load(material_scene());
    REQUIRE_MESSAGE(loaded.ok, loaded.error);
    REQUIRE(loaded.scene.images.size() == 1);
    const auto& picture = loaded.scene.images.front();
    CHECK(picture.file_extension == ".webp");
    const auto header = EncodedImage::read(picture.encoded.data(), picture.encoded.size());
    REQUIRE(header.has_value());
    CHECK(header->format() != EncodedImageFormat::Webp);
  }
}
