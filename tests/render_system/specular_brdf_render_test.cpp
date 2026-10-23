#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "render/shader_source.hpp"
#include "render/shaders/lit_fragment.hpp"
#include "render_system/render_stage.hpp"

namespace {

constexpr int kSamples = 512;
constexpr int kChannels = 3;
constexpr int kProbeHeight = 1;
constexpr float kHalfSample = 0.5f;
constexpr double kReferencePi = 3.14159265358979323846;
constexpr std::size_t kRoughnessCount = 4;
constexpr std::array<float, kRoughnessCount> kRoughness{0.045f, 0.07f, 0.15f, 0.4f};
constexpr ::Color kWhite{255, 255, 255, 255};
constexpr ::Color kBlack{0, 0, 0, 255};
constexpr const char* kReferenceSampler = "referenceSamples";
constexpr const char* kProbe = R"GLSL(
uniform sampler2D referenceSamples;
const int NDF_PROBE_SAMPLES = 512;
const float NDF_PROBE_TOLERANCE = 0.015;  // float texture quantization near the narrowest supported lobe
const int NDF_PROBE_ROUGHNESS_COUNT = 4;
const float NDF_PROBE_ROUGHNESS[NDF_PROBE_ROUGHNESS_COUNT] = float[NDF_PROBE_ROUGHNESS_COUNT](0.045, 0.07, 0.15, 0.4);
bool ndfProbeNormalized(float value) { return abs(value - 1.0) <= NDF_PROBE_TOLERANCE; }
void main() {
  int row = int(gl_FragCoord.x);
  float rough = NDF_PROBE_ROUGHNESS[row];
  float alpha = rough * rough;
  float peak = texelFetch(referenceSamples, ivec2(0, row), 0).b;
  float mass = 0.0;
  for (int sampleIndex = 0; sampleIndex < NDF_PROBE_SAMPLES; ++sampleIndex) {
    vec2 sampleValue = texelFetch(referenceSamples, ivec2(sampleIndex, row), 0).rg;
    mass += distributionGGX(sampleValue.x, alpha) / sampleValue.y;
  }
  bool correctPeak = ndfProbeNormalized(distributionGGX(1.0, alpha) / peak);
  bool correctMass = ndfProbeNormalized(mass / float(NDF_PROBE_SAMPLES));
  finalColor = vec4(correctPeak ? 1.0 : 0.0, correctMass ? 1.0 : 0.0, 1.0, 1.0);
}
)GLSL";

std::array<float, kChannels> reference_sample(float roughness, int index) {
  const double alpha = static_cast<double>(roughness) * roughness;
  const double alpha2 = alpha * alpha;
  const double u = (static_cast<double>(index) + kHalfSample) / kSamples;
  const auto cosine = static_cast<float>(std::sqrt((1.0 - u) / (1.0 + (alpha2 - 1.0) * u)));
  const double c = cosine;  // evaluate the independent density at the quantized direction the GPU receives
  const double sine2 = (1.0 - c) * (1.0 + c);
  const double denominator = sine2 + alpha2 * c * c;
  const double density = alpha2 / (kReferencePi * denominator * denominator);
  return {cosine, static_cast<float>(density), static_cast<float>(1.0 / (kReferencePi * alpha2))};
}

std::vector<float> reference_pixels() {
  std::vector<float> pixels;
  for (const float roughness : kRoughness) {
    for (int index = 0; index < kSamples; ++index) {
      const auto sample = reference_sample(roughness, index);
      pixels.insert(pixels.end(), sample.begin(), sample.end());
    }
  }
  return pixels;
}

Texture2D reference_texture() {
  auto pixels = reference_pixels();
  const Image image{pixels.data(), kSamples, static_cast<int>(kRoughness.size()), 1,
                    PIXELFORMAT_UNCOMPRESSED_R32G32B32};
  Texture2D texture = LoadTextureFromImage(image);
  SetTextureFilter(texture, TEXTURE_FILTER_POINT);
  return texture;
}

struct Probe {
  Shader shader = LoadShaderFromMemory(
      nullptr, redgem::shaders::for_this_gpu((std::string(redgem::shaders::kLitHead) + kProbe).c_str()).c_str());
  Texture2D samples = reference_texture();
  RenderTexture2D target = LoadRenderTexture(static_cast<int>(kRoughness.size()), kProbeHeight);

  Probe() = default;
  Probe(const Probe&) = delete;
  Probe& operator=(const Probe&) = delete;
  ~Probe() {
    UnloadRenderTexture(target);
    UnloadTexture(samples);
    UnloadShader(shader);
  }
};

void draw_probe(const Probe& probe) {
  BeginDrawing();
  BeginTextureMode(probe.target);
  ClearBackground(kBlack);
  BeginShaderMode(probe.shader);
  SetShaderValueTexture(probe.shader, GetShaderLocation(probe.shader, kReferenceSampler), probe.samples);
  DrawRectangle(0, 0, static_cast<int>(kRoughness.size()), kProbeHeight, kWhite);
  EndShaderMode();
  EndTextureMode();
  EndDrawing();
}

std::vector<::Color> probe_pixels(const Probe& probe) {
  Image image = LoadImageFromTexture(probe.target.texture);
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  const auto* data = static_cast<const ::Color*>(image.data);
  const std::vector<::Color> pixels(data, data + kRoughness.size());
  UnloadImage(image);
  return pixels;
}

}  // namespace

TEST_CASE("GGX preserves the analytic peak and projected mass of smooth specular lobes") {
  redgem::test::RenderStage stage;
  const Probe probe;
  REQUIRE(GetShaderLocation(probe.shader, kReferenceSampler) >= 0);
  draw_probe(probe);
  const auto pixels = probe_pixels(probe);
  for (std::size_t index = 0; index < pixels.size(); ++index) {
    INFO("Roughness: ", kRoughness[index]);
    CHECK(pixels[index].r == kWhite.r);
    CHECK(pixels[index].g == kWhite.g);
  }
}
