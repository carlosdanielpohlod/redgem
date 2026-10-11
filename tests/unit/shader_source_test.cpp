#include <doctest/doctest.h>

#include <string>

#include "core/config.hpp"
#include "render/shader_source.hpp"
#include "render/shaders.hpp"

using namespace redgem;

namespace {

constexpr int kBones = 48;
constexpr int kLights = 16;
constexpr const char* kBody = "\nuniform mat4 boneMatrices[MAX_BONES];\nvoid main() {}\n";
constexpr const char* kNote = "// a note before the version\n";
constexpr const char* kBoneDefine = "\n#define MAX_BONES 48";
constexpr const char* kLightDefine = "\n#define MAX_POINT_LIGHTS 16";
constexpr const char* kNoVersion = "void main() {}";
constexpr const char* kVersionWord = "#version";
constexpr const char* kDefineWord = "#define ";
// Every shader the engine compiles.
constexpr const char* kEngineShaders[] = {shaders::kLitVertex,         shaders::kLitFragment,
                                          shaders::kDepthFragment,     shaders::kSkyVertex,
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
  const std::string prepared = shaders::with_header(desktop_source(), shaders::kMobileHeader, kBones, kLights);
  CHECK(starts_with(prepared, shaders::kMobileHeader));
  CHECK_FALSE(contains(prepared, shaders::kDesktopVersionLine));
  CHECK_FALSE(contains(prepared, kNote));  // the version must be the very first line
}

TEST_CASE("on desktop a shader keeps its version line, moved to the very start") {
  const std::string prepared = shaders::with_header(desktop_source(), shaders::kDesktopVersionLine, kBones, kLights);
  CHECK(starts_with(prepared, shaders::kDesktopVersionLine));
  CHECK_FALSE(contains(prepared, kNote));
}

TEST_CASE("a shader learns the engine's limits right after its header, before its own code") {
  const std::string prepared = shaders::with_header(desktop_source(), shaders::kMobileHeader, kBones, kLights);
  const std::string expected = std::string(shaders::kMobileHeader) + kBoneDefine + kLightDefine + kBody;
  CHECK(prepared == expected);
}

TEST_CASE("text without a version line is left as it is") {
  CHECK(shaders::with_header(kNoVersion, shaders::kMobileHeader, kBones, kLights) == kNoVersion);
}

TEST_CASE("every engine shader is prepared with a version first and the engine's own limits") {
  const std::string bones =
      std::string(kDefineWord) + shaders::kBoneLimitName + " " + std::to_string(config::kMaxSkinJoints) + "\n";
  const std::string lights =
      std::string(kDefineWord) + shaders::kPointLightLimitName + " " + std::to_string(shaders::kMaxPointLights) + "\n";
  for (const char* source : kEngineShaders) {
    const std::string prepared = shaders::for_this_gpu(source);
    CHECK(starts_with(prepared, kVersionWord));
    CHECK(contains(prepared, bones));
    CHECK(contains(prepared, lights));
  }
}

TEST_CASE("no engine shader sets a limit of its own that could disagree with the engine's") {
  for (const char* source : kEngineShaders) {
    CHECK_FALSE(contains(source, std::string(kDefineWord) + shaders::kPointLightLimitName));
    CHECK_FALSE(contains(source, std::string(kDefineWord) + shaders::kBoneLimitName));
  }
}
