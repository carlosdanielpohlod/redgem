#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <regex>
#include <string>
#include <vector>

#include "render/gpu_profile.hpp"
#include "render/shader_source.hpp"
#include "render/shaders/ambient_occlusion.hpp"
#include "render/shaders/antialias.hpp"
#include "render/shaders/bloom.hpp"
#include "render/shaders/lit_fragment.hpp"
#include "render/shaders/lit_vertex.hpp"
#include "render/shaders/motion.hpp"
#include "render/shaders/panorama.hpp"
#include "render/shaders/particle.hpp"
#include "render/shaders/shadow_depth.hpp"
#include "render/shaders/sky.hpp"
#include "render/shaders/temporal.hpp"
#include "render/shaders/tone_mapping.hpp"
#include "render/shaders/uniforms.hpp"
#include "render/shaders/water_waves.hpp"
#include "render/shaders/water_filter.hpp"
#include "render/texel_rows.hpp"

using namespace redgem;

namespace {

constexpr int kRowTexels = 64;
constexpr int kLights = 16;
constexpr const char* kBody = "\nint row(int texel) { return texel / TEXELS_PER_ROW; }\nvoid main() {}\n";
constexpr const char* kNote = "// a note before the version\n";
constexpr const char* kRowDefine = "\n#define TEXELS_PER_ROW 64";
constexpr const char* kLightDefine = "\n#define MAX_POINT_LIGHTS 16";
constexpr const char* kNoVersion = "void main() {}";
constexpr const char* kVersionWord = "#version";
constexpr const char* kDefineWord = "#define ";
// Every shader the engine compiles.
const std::string kEngineShaders[] = {shaders::lit_vertex(),       shaders::lit_fragment(),
                                      shaders::depth_fragment(),   shaders::kSkyVertex,
                                      shaders::kSkyFragment,       shaders::particle_fragment(),
                                      shaders::kPanoramaFragment,  shaders::kAmbientOcclusionFragment,
                                      shaders::kBrightFragment,    shaders::kBlurFragment,
                                      shaders::kCompositeFragment, shaders::kAntialiasFragment,
                                      shaders::motion_vertex(),    shaders::motion_fragment(),
                                      shaders::kTemporalFragment,  shaders::kSharpenFragment};

std::string desktop_source() { return std::string(kNote) + shaders::kDesktopVersionLine + kBody; }

bool starts_with(const std::string& text, const std::string& start) {
  return text.compare(0, start.size(), start) == 0;
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("on phones a shader starts with their header instead of the desktop version") {
  const std::string prepared = shaders::with_header(desktop_source(), shaders::kMobileHeader, kRowTexels, kLights);
  CHECK(starts_with(prepared, shaders::kMobileHeader));
  CHECK_FALSE(contains(prepared, shaders::kDesktopVersionLine));
  CHECK_FALSE(contains(prepared, kNote));  // the version must be the very first line
}

TEST_CASE("on desktop a shader keeps its version line, moved to the very start") {
  const std::string prepared =
      shaders::with_header(desktop_source(), shaders::kDesktopVersionLine, kRowTexels, kLights);
  CHECK(starts_with(prepared, shaders::kDesktopVersionLine));
  CHECK_FALSE(contains(prepared, kNote));
}

TEST_CASE("a shader learns the engine's limits right after its header, before its own code") {
  const std::string prepared = shaders::with_header(desktop_source(), shaders::kMobileHeader, kRowTexels, kLights);
  const std::string expected = std::string(shaders::kMobileHeader) + kRowDefine + kLightDefine + kBody;
  CHECK(prepared == expected);
}

TEST_CASE("text without a version line is left as it is") {
  CHECK(shaders::with_header(kNoVersion, shaders::kMobileHeader, kRowTexels, kLights) == kNoVersion);
}

TEST_CASE("every engine shader is prepared with a version first and the engine's own numbers") {
  const std::string rows =
      std::string(kDefineWord) + shaders::kTexelRowName + " " + std::to_string(render_math::kTexelsPerRow) + "\n";
  const std::string lights =
      std::string(kDefineWord) + shaders::kPointLightLimitName + " " + std::to_string(shaders::kMaxPointLights) + "\n";
  for (const std::string& source : kEngineShaders) {
    const std::string prepared = shaders::for_this_gpu(source.c_str());
    CHECK(starts_with(prepared, kVersionWord));
    CHECK(contains(prepared, rows));
    CHECK(contains(prepared, lights));
  }
}

TEST_CASE("no engine shader sets a limit of its own that could disagree with the engine's") {
  for (const std::string& source : kEngineShaders) {
    CHECK_FALSE(contains(source, std::string(kDefineWord) + shaders::kPointLightLimitName));
    CHECK_FALSE(contains(source, std::string(kDefineWord) + shaders::kTexelRowName));
  }
}

TEST_CASE("the shaders start with what this build's graphics API wants") {
  const std::string prepared = shaders::for_this_gpu(shaders::lit_vertex().c_str());
  CHECK(starts_with(prepared, gpu_profile::kShaderHeader));
}

TEST_CASE("the lit vertex shader holds no array of bones: a skeleton's bones are fetched from the frame's texels") {
  CHECK_FALSE(contains(shaders::lit_vertex(), "boneMatrices["));
  CHECK(contains(shaders::lit_vertex(), std::string("uniform sampler2D ") + shaders::kFrameTexels));
  CHECK(contains(shaders::lit_vertex(), "texelFetch"));
}

TEST_CASE("the motion pass places each vertex as the lit pass does, from this frame's texels and the last one's") {
  const std::string motion = shaders::motion_vertex();
  CHECK(contains(motion, shaders::kVertexPlacement));
  CHECK(contains(shaders::lit_vertex(), shaders::kVertexPlacement));
  CHECK(contains(motion, std::string("uniform sampler2D ") + shaders::kPreviousTexels));
}

TEST_CASE("the lit pass and the shadow map's leave out the same pixels of a fading copy, written once") {
  constexpr const char* kNoiseName = "NOISE_WEIGHTS =";
  for (const std::string& source : {shaders::lit_fragment(), shaders::depth_fragment()}) {
    CHECK(source.find(shaders::kCopyFade) != std::string::npos);
    CHECK(source.find(kNoiseName) == source.rfind(kNoiseName));  // defined once
  }
}

TEST_CASE("the coating shader is enabled only when a scene asks for layered materials") {
  const shaders::LitFeatures plain;
  const shaders::LitFeatures coated{false, false, true};
  CHECK(plain != coated);
  CHECK_FALSE(contains(shaders::lit_fragment(plain), shaders::kClearcoatSwitch));
  CHECK(contains(shaders::lit_fragment(coated), shaders::kClearcoatSwitch));
}

namespace {

constexpr std::size_t kMinimumWaterComponents = 6;
constexpr std::size_t kMaximumWaterComponents = 8;
constexpr std::size_t kWaveFields = 4;
constexpr std::size_t kWaveLengthField = 2;
constexpr std::size_t kWaveWeightField = 3;
constexpr float kDominantWaterX = 0.8f;
constexpr float kDominantWaterZ = 0.6f;
constexpr float kMinimumDirectionAgreement = 0.9f;  // less than 26 degrees from the dominant direction
constexpr float kMinimumWavelengthRatio = 1.15f;    // distinct scales, rather than almost duplicated carriers
constexpr float kMinimumDirectionSpread = 0.5f;     // waves spread on both sides of the dominant direction
using WaterComponent = std::array<float, kWaveFields>;

std::vector<WaterComponent> water_components() {
  const std::string source = shaders::kWaterWaves;
  const std::regex declaration(R"(WATER_COMPONENT_[0-9]+ = vec4\(([^,]+), ([^,]+), ([^,]+), ([^)]+)\))");
  std::vector<WaterComponent> components;
  for (auto it = std::sregex_iterator(source.begin(), source.end(), declaration); it != std::sregex_iterator(); ++it) {
    WaterComponent component{};
    for (std::size_t field = 0; field < kWaveFields; ++field) component[field] = std::stof((*it)[field + 1].str());
    components.push_back(component);
  }
  return components;
}

float water_direction_offset(const WaterComponent& component) {
  return kDominantWaterX * component[1] - kDominantWaterZ * component[0];
}

}  // namespace

TEST_CASE("water spectrum bounds displacement with normalized positive weights and distinct length scales") {
  const auto components = water_components();
  REQUIRE(components.size() >= kMinimumWaterComponents);
  CHECK(components.size() <= kMaximumWaterComponents);
  float weight = 0.0f;
  float previous_length = std::numeric_limits<float>::infinity();
  for (const auto& component : components) {
    CHECK(component[kWaveWeightField] > 0.0f);
    CHECK(component[kWaveLengthField] > 0.0f);
    CHECK(previous_length / component[kWaveLengthField] > kMinimumWavelengthRatio);
    weight += component[kWaveWeightField];
    previous_length = component[kWaveLengthField];
  }
  CHECK(weight == doctest::Approx(1.0f));
}

TEST_CASE("water carriers share one prevailing direction without forming a perpendicular wave grid") {
  const auto components = water_components();
  REQUIRE(components.size() >= kMinimumWaterComponents);
  float lowest = 1.0f;
  float highest = -1.0f;
  for (const auto& component : components) {
    CHECK(component[0] * component[0] + component[1] * component[1] == doctest::Approx(1.0f));
    CHECK(component[0] * kDominantWaterX + component[1] * kDominantWaterZ >= kMinimumDirectionAgreement);
    lowest = std::min(lowest, water_direction_offset(component));
    highest = std::max(highest, water_direction_offset(component));
  }
  CHECK(highest - lowest >= kMinimumDirectionSpread);
}

TEST_CASE("water normal filtering is fragment only and keeps vertex and motion geometry unfiltered") {
  shaders::LitFeatures water;
  water.water = true;
  CHECK(contains(shaders::lit_fragment(water), shaders::kWaterFilter));
  CHECK_FALSE(contains(shaders::lit_fragment(), shaders::kWaterFilter));
  CHECK_FALSE(contains(shaders::lit_vertex(), shaders::kWaterFilter));
  CHECK_FALSE(contains(shaders::motion_vertex(), shaders::kWaterFilter));
  CHECK(contains(shaders::lit_vertex(), shaders::kWaterWaves));
  CHECK(contains(shaders::motion_vertex(), shaders::kWaterWaves));
}
