#include <doctest/doctest.h>

#include <string>

#include "render/gpu_profile.hpp"
#include "render/shader_source.hpp"
#include "render/shaders/ambient_occlusion.hpp"
#include "render/shaders/antialias.hpp"
#include "render/shaders/bloom.hpp"
#include "render/shaders/lit_fragment.hpp"
#include "render/shaders/lit_vertex.hpp"
#include "render/shaders/panorama.hpp"
#include "render/shaders/particle.hpp"
#include "render/shaders/shadow_depth.hpp"
#include "render/shaders/sky.hpp"
#include "render/shaders/tone_mapping.hpp"
#include "render/shaders/uniforms.hpp"
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
const std::string kEngineShaders[] = {shaders::kLitVertex,         shaders::lit_fragment(),
                                      shaders::depth_fragment(),   shaders::kSkyVertex,
                                      shaders::kSkyFragment,       shaders::kParticleFragment,
                                      shaders::kPanoramaFragment,  shaders::kAmbientOcclusionFragment,
                                      shaders::kBrightFragment,    shaders::kBlurFragment,
                                      shaders::kCompositeFragment, shaders::kAntialiasFragment};

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
  const std::string prepared = shaders::for_this_gpu(shaders::kLitVertex);
  CHECK(starts_with(prepared, gpu_profile::kShaderHeader));
}

TEST_CASE("the lit vertex shader holds no array of bones: a skeleton's bones are fetched from the frame's texels") {
  CHECK_FALSE(contains(shaders::kLitVertex, "boneMatrices["));
  CHECK(contains(shaders::kLitVertex, std::string("uniform sampler2D ") + shaders::kFrameTexels));
  CHECK(contains(shaders::kLitVertex, "texelFetch"));
}

TEST_CASE("the lit pass and the shadow map's leave out the same pixels of a fading copy, written once") {
  constexpr const char* kNoiseName = "NOISE_WEIGHTS =";
  for (const std::string& source : {shaders::lit_fragment(), shaders::depth_fragment()}) {
    CHECK(source.find(shaders::kCopyFade) != std::string::npos);
    CHECK(source.find(kNoiseName) == source.rfind(kNoiseName));  // defined once
  }
}
