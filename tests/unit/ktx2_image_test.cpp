#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/gpu_texture_formats.hpp"
#include "core/image_header.hpp"
#include "core/ktx2_header.hpp"
#include "core/ktx2_image.hpp"
#include "core/safe_path.hpp"
#include "scene/gltf/gltf_budget.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "test_support.hpp"

using namespace redgem;
using nlohmann::json;

namespace {

// tests/fixtures/images/*.ktx2, made by tools/assets/make_ktx2_fixtures.py.
constexpr const char* kOpaqueFixture = "etc1s_opaque";  // ETC1S, 16 x 8, red left half, blue right half
constexpr const char* kAlphaFixture = "uastc_alpha";    // UASTC + Zstandard, 8 x 8, white, see-through bottom half
constexpr std::uint32_t kOpaqueWidth = 16;
constexpr std::uint32_t kOpaqueHeight = 8;
constexpr std::uint32_t kOpaqueLevels = 5;  // 16x8, 8x4, 4x2, 2x1, 1x1
constexpr std::uint32_t kAlphaSide = 8;
constexpr std::uint32_t kAlphaLevels = 4;  // 8x8, 4x4, 2x2, 1x1
constexpr std::size_t kRgbaBytes = 4;
constexpr std::uint32_t kBlockSide = 4;
constexpr std::size_t kSmallBlockBytes = 8;   // BC1, ETC1
constexpr std::size_t kLargeBlockBytes = 16;  // BC3, ASTC 4x4, ETC2 with alpha
// What a compressed picture's colors come back as, give or take its loss.
constexpr int kBright = 200;
constexpr int kDark = 60;
// Header fields the refusal cases change (see ktx2_header.cpp).
constexpr std::size_t kVkFormatAt = 12;
constexpr std::size_t kDepthAt = 28;
constexpr std::size_t kLayerCountAt = 32;
constexpr std::size_t kFaceCountAt = 36;
constexpr std::size_t kLevelCountAt = 40;
constexpr std::size_t kSupercompressionAt = 44;
constexpr std::size_t kLevelIndexAt = 80;
constexpr std::uint8_t kSomeVkFormat = 37;  // VK_FORMAT_R8G8B8A8_UNORM
constexpr std::uint8_t kCubeFaces = 6;
constexpr std::uint8_t kTwo = 2;
constexpr std::uint8_t kUnknownSupercompression = 3;
constexpr std::uint8_t kTooManyLevels = kOpaqueLevels + 1;

std::vector<std::uint8_t> ktx2_bytes(const std::string& name) {
  const auto bytes = read_file_up_to(test::fixture("images/" + name + ".ktx2"), kMaxEncodedImageBytes);
  REQUIRE(bytes.has_value());
  return *bytes;
}

std::size_t blocks(ImageSize size) {
  return std::size_t{(size.width + kBlockSide - 1) / kBlockSide} * ((size.height + kBlockSide - 1) / kBlockSide);
}

std::size_t expected_bytes(GpuBlockFormat format, ImageSize size) {
  switch (format) {
    case GpuBlockFormat::Bc1Rgb:
    case GpuBlockFormat::Etc1Rgb:
      return blocks(size) * kSmallBlockBytes;
    case GpuBlockFormat::Rgba8:
      return std::size_t{size.width} * size.height * kRgbaBytes;
    default:
      return blocks(size) * kLargeBlockBytes;
  }
}

// Every level is half the one before it (never under a pixel), holding exactly what its format takes.
void check_levels(const TranscodedPicture& picture, ImageSize full, std::uint32_t levels) {
  REQUIRE(picture.levels.size() == levels);
  ImageSize size = full;
  for (const TranscodedLevel& level : picture.levels) {
    CHECK(level.size.width == size.width);
    CHECK(level.size.height == size.height);
    CHECK(level.bytes.size() == expected_bytes(picture.format, size));
    size = ImageSize{std::max(size.width / kTwo, 1u), std::max(size.height / kTwo, 1u)};
  }
}

const std::uint8_t* pixel(const TranscodedLevel& level, std::uint32_t x, std::uint32_t y) {
  return level.bytes.data() + (std::size_t{y} * level.size.width + x) * kRgbaBytes;
}

bool reads(std::vector<std::uint8_t> bytes) { return read_ktx2_header(bytes.data(), bytes.size()).has_value(); }

std::vector<std::uint8_t> with_byte(std::vector<std::uint8_t> bytes, std::size_t at, std::uint8_t value) {
  bytes[at] = value;
  return bytes;
}

struct Ktx2Scene {
  std::filesystem::path folder = std::filesystem::temp_directory_path() / "redgem-ktx2-scene-test";
  Ktx2Scene() {
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    std::filesystem::copy_file(test::fixture("images/uastc_alpha.ktx2"), folder / "picture.ktx2");
    std::filesystem::copy_file(test::fixture("images/sniff.png"), folder / "fallback.png");
  }
  ~Ktx2Scene() { std::filesystem::remove_all(folder); }
  LoadResult load(const json& doc) const {
    const auto file = folder / "scene.gltf";
    std::ofstream(file) << doc.dump();
    return load_gltf_scene(file.string());
  }
};

json basisu_scene() {
  return {{"asset", {{"version", "2.0"}}},
          {"scene", 0},
          {"scenes", json::array({{{"nodes", {0}}}})},
          {"nodes", json::array({{{"name", "Root"}}})},
          {"extensionsUsed", {"KHR_texture_basisu"}},
          {"images", json::array({{{"uri", "fallback.png"}}, {{"uri", "picture.ktx2"}, {"mimeType", "image/ktx2"}}})},
          {"textures", json::array({{{"source", 0}, {"extensions", {{"KHR_texture_basisu", {{"source", 1}}}}}}})},
          {"materials", json::array({{{"pbrMetallicRoughness", {{"baseColorTexture", {{"index", 0}}}}}}})}};
}

}  // namespace

TEST_CASE("a KTX2 header tells the picture's size, levels and supercompression") {
  const auto opaque = ktx2_bytes(kOpaqueFixture);
  const auto header = read_ktx2_header(opaque.data(), opaque.size());
  REQUIRE(header.has_value());
  CHECK(header->size.width == kOpaqueWidth);
  CHECK(header->size.height == kOpaqueHeight);
  CHECK(header->levels == kOpaqueLevels);
  CHECK(header->supercompression == Ktx2Supercompression::BasisLz);
  const auto alpha = ktx2_bytes(kAlphaFixture);
  const auto alpha_header = read_ktx2_header(alpha.data(), alpha.size());
  REQUIRE(alpha_header.has_value());
  CHECK(alpha_header->levels == kAlphaLevels);
  CHECK(alpha_header->supercompression == Ktx2Supercompression::Zstandard);
  CHECK(starts_as_ktx2(alpha.data(), alpha.size()));
}

TEST_CASE("KTX2 headers of what the engine does not draw, or that lie, are refused") {
  const auto good = ktx2_bytes(kOpaqueFixture);
  REQUIRE(reads(good));
  CHECK_FALSE(reads(with_byte(good, 0, 0)));
  CHECK_FALSE(reads(with_byte(good, kVkFormatAt, kSomeVkFormat)));
  CHECK_FALSE(reads(with_byte(good, kDepthAt, 1)));
  CHECK_FALSE(reads(with_byte(good, kLayerCountAt, kTwo)));
  CHECK_FALSE(reads(with_byte(good, kFaceCountAt, kCubeFaces)));
  CHECK_FALSE(reads(with_byte(good, kLevelCountAt, kTooManyLevels)));
  CHECK_FALSE(reads(with_byte(good, kSupercompressionAt, kUnknownSupercompression)));
  CHECK_FALSE(reads(std::vector<std::uint8_t>(good.begin(), good.begin() + kLevelIndexAt)));
  CHECK_FALSE(reads(std::vector<std::uint8_t>(good.begin(), good.end() - 1)));  // its last level cut short
  CHECK_FALSE(read_ktx2_header(nullptr, 0).has_value());
  CHECK_FALSE(starts_as_ktx2(good.data(), kLevelIndexAt - 1));
}

TEST_CASE("the extensions a card lists pick the compressed format a picture is transcoded to") {
  const GpuTextureSupport desktop = support_from_extensions({"GL_EXT_texture_compression_s3tc"}, false);
  CHECK(best_block_format(desktop, false) == GpuBlockFormat::Bc1Rgb);
  CHECK(best_block_format(desktop, true) == GpuBlockFormat::Bc3Rgba);
  const GpuTextureSupport phone = support_from_extensions({"GL_KHR_texture_compression_astc_ldr"}, true);
  CHECK(phone.etc2);
  CHECK(best_block_format(phone, true) == GpuBlockFormat::Astc4x4Rgba);
  const GpuTextureSupport older_phone = support_from_extensions({}, true);
  CHECK(best_block_format(older_phone, false) == GpuBlockFormat::Etc1Rgb);
  CHECK(best_block_format(older_phone, true) == GpuBlockFormat::Etc2Rgba);
  CHECK(support_from_extensions({"GL_ARB_ES3_compatibility"}, false).etc2);
  const GpuTextureSupport none = support_from_extensions({"GL_ARB_texture_float"}, false);
  CHECK(best_block_format(none, true) == GpuBlockFormat::Rgba8);
  CHECK_FALSE(is_block_compressed(GpuBlockFormat::Rgba8));
  CHECK(is_block_compressed(GpuBlockFormat::Astc4x4Rgba));
}

TEST_CASE("ETC1S and UASTC pictures transcode every mipmap level to each format a card may take") {
  const auto opaque = ktx2_bytes(kOpaqueFixture);
  const auto alpha = ktx2_bytes(kAlphaFixture);
  const GpuTextureSupport supports[] = {GpuTextureSupport{true, false, false}, GpuTextureSupport{false, true, false},
                                        GpuTextureSupport{false, false, true}, GpuTextureSupport{}};
  for (const GpuTextureSupport& support : supports) {
    const auto flat = transcode_ktx2(opaque.data(), opaque.size(), support);
    REQUIRE(flat.has_value());
    CHECK_FALSE(flat->has_alpha);
    CHECK(flat->format == best_block_format(support, false));
    check_levels(*flat, ImageSize{kOpaqueWidth, kOpaqueHeight}, kOpaqueLevels);
    const auto clear = transcode_ktx2(alpha.data(), alpha.size(), support);
    REQUIRE(clear.has_value());
    CHECK(clear->has_alpha);
    CHECK(clear->format == best_block_format(support, true));
    check_levels(*clear, ImageSize{kAlphaSide, kAlphaSide}, kAlphaLevels);
  }
}

TEST_CASE("a KTX2 picture decodes to RGBA8 pixels close to what was compressed") {
  const auto opaque = ktx2_bytes(kOpaqueFixture);
  const auto flat = decode_ktx2_rgba(opaque.data(), opaque.size());
  REQUIRE(flat.has_value());
  REQUIRE(flat->bytes.size() == std::size_t{kOpaqueWidth} * kOpaqueHeight * kRgbaBytes);
  const std::uint8_t* left = pixel(*flat, 1, 1);
  const std::uint8_t* right = pixel(*flat, kOpaqueWidth - 2, kOpaqueHeight - 2);
  CHECK(left[0] > kBright);
  CHECK(left[2] < kDark);
  CHECK(right[0] < kDark);
  CHECK(right[2] > kBright);
  const auto alpha = ktx2_bytes(kAlphaFixture);
  const auto clear = decode_ktx2_rgba(alpha.data(), alpha.size());
  REQUIRE(clear.has_value());
  CHECK(pixel(*clear, 1, 1)[3] > kBright);
  CHECK(pixel(*clear, 1, kAlphaSide - 2)[3] < kDark);
  CHECK(pixel(*clear, 1, 1)[1] > kBright);
}

TEST_CASE("files that are not KTX2 pictures the engine reads are not transcoded") {
  const auto opaque = ktx2_bytes(kOpaqueFixture);
  const std::vector<std::uint8_t> cut(opaque.begin(), opaque.begin() + kLevelIndexAt);
  CHECK_FALSE(transcode_ktx2(cut.data(), cut.size(), GpuTextureSupport{}).has_value());
  CHECK_FALSE(decode_ktx2_rgba(nullptr, 0).has_value());
  const auto png = read_file_up_to(test::fixture("images/sniff.png"), kMaxEncodedImageBytes);
  REQUIRE(png.has_value());
  CHECK_FALSE(decode_ktx2_rgba(png->data(), png->size()).has_value());
}

TEST_CASE("a KTX2 picture counts toward a scene's decoded memory as an RGBA8 one") {
  const auto opaque = ktx2_bytes(kOpaqueFixture);
  CHECK(gltf_budget::decoded_bytes({ImageData{opaque, ".ktx2"}}) ==
        std::uint64_t{kOpaqueWidth} * kOpaqueHeight * kRgbaBytes);
}

TEST_CASE("glTF KHR_texture_basisu shows its KTX2 picture over the plain fallback") {
  Ktx2Scene scene;
  auto doc = basisu_scene();
  SUBCASE("with a mime type") {}
  SUBCASE("named by its file alone") { doc["images"][1].erase("mimeType"); }
  const auto loaded = scene.load(doc);
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  REQUIRE(loaded.scene.materials.size() == 1);
  CHECK(loaded.scene.materials[0].base_color_image == 1);
  REQUIRE(loaded.scene.images.size() == 2);
  CHECK(loaded.scene.images[1].file_extension == ".ktx2");
  CHECK(starts_as_ktx2(loaded.scene.images[1].encoded.data(), loaded.scene.images[1].encoded.size()));
}
