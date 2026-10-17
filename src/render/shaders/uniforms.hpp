#pragma once

// What the GLSL and the renderer have to agree on: the names of the uniforms and the sizes of
// the arrays.

namespace redgem::shaders {

// Uniform names shared between the GLSL below and the renderer.
inline constexpr const char* kSunDirection = "sunDirection";
inline constexpr const char* kSunColor = "sunColor";
inline constexpr const char* kAmbientStrength = "ambientStrength";
inline constexpr const char* kPointLightCount = "pointLightCount";
inline constexpr const char* kPointLightPositions = "pointLightPositions";
inline constexpr const char* kPointLightColors = "pointLightColors";
inline constexpr const char* kPointLightRanges = "pointLightRanges";
inline constexpr const char* kFogColor = "fogColor";
inline constexpr const char* kFogDensity = "fogDensity";
inline constexpr const char* kViewPosition = "viewPos";
inline constexpr const char* kMetallic = "metallic";
inline constexpr const char* kRoughness = "roughness";
inline constexpr const char* kEmissive = "emissive";
inline constexpr const char* kEmissiveMap = "emissiveMap";
inline constexpr const char* kHasEmissiveMap = "hasEmissiveMap";
inline constexpr const char* kNormalMap = "normalMap";
inline constexpr const char* kHasNormalMap = "hasNormalMap";
inline constexpr const char* kNormalScale = "normalScale";
inline constexpr const char* kSurfaceMap = "surfaceMap";
inline constexpr const char* kHasSurfaceMap = "hasSurfaceMap";
inline constexpr const char* kOcclusionMap = "occlusionMap";
inline constexpr const char* kOcclusionStrength = "occlusionStrength";
inline constexpr const char* kShadowMap = "shadowMap";
inline constexpr const char* kLightViewProjection = "lightViewProjection";
inline constexpr const char* kShadowStrength = "shadowStrength";
inline constexpr const char* kShadowTexel = "shadowTexel";
inline constexpr const char* kShadowDepthRange = "shadowDepthRange";
inline constexpr const char* kFrameTexels = "frameTexels";
// The vertex attribute a copy of a mesh finds its record by (see instance_records.hpp).
inline constexpr const char* kInstanceSlot = "instanceSlot";
inline constexpr const char* kSkinned = "skinned";
inline constexpr const char* kReflectionMap = "reflectionMap";
inline constexpr const char* kReflectionStrength = "reflectionStrength";
inline constexpr const char* kFaceForward = "faceForward";
inline constexpr const char* kFaceRight = "faceRight";
inline constexpr const char* kFaceUp = "faceUp";
inline constexpr const char* kTargetSize = "targetSize";
inline constexpr const char* kAlphaCutoff = "alphaCutoff";
inline constexpr const char* kAlphaCeiling = "alphaCeiling";
inline constexpr const char* kAlphaDithered = "alphaDithered";
inline constexpr const char* kAlphaMixed = "alphaMixed";
inline constexpr const char* kWindAmount = "windAmount";
inline constexpr const char* kTime = "time";
inline constexpr const char* kSkyTop = "skyTop";
inline constexpr const char* kSkyHorizon = "skyHorizon";
inline constexpr const char* kGroundColor = "groundColor";
inline constexpr const char* kSunGlow = "sunGlow";
inline constexpr const char* kAlphaToCoverage = "alphaToCoverage";
// How the pixels' fine pattern (fading copies, dithered surfaces) is shifted this frame.
inline constexpr const char* kNoiseShift = "noiseShift";
// The sky as a picture.
inline constexpr const char* kSkyLight = "skyLight";
inline constexpr const char* kSkyIrradiance = "skyIrradiance";
inline constexpr const char* kSkyReflections = "skyReflections";
inline constexpr const char* kSkyReflectance = "skyReflectance";
inline constexpr const char* kSkyTurn = "skyTurn";
inline constexpr const char* kSkyIntensity = "skyIntensity";
// Shadow cascades.
inline constexpr const char* kShadowTiles = "shadowTiles";
inline constexpr const char* kShadowScales = "shadowScales";
inline constexpr const char* kShadowCascades = "shadowCascades";
inline constexpr const char* kSunSize = "sunSize";
// Post-processing.
inline constexpr const char* kThreshold = "threshold";
inline constexpr const char* kBlurStep = "blurStep";
inline constexpr const char* kBloomTexture = "bloomTexture";
inline constexpr const char* kBloomStrength = "bloomStrength";
inline constexpr const char* kExposure = "exposure";
inline constexpr const char* kVignette = "vignette";
inline constexpr const char* kTexelSize = "texelSize";
inline constexpr const char* kTanHalfFov = "tanHalfFov";
inline constexpr const char* kAspect = "aspect";
inline constexpr const char* kNearPlane = "nearPlane";
inline constexpr const char* kFarPlane = "farPlane";
inline constexpr const char* kOcclusionTexture = "occlusionTexture";
inline constexpr const char* kOcclusionAmount = "occlusionAmount";
inline constexpr const char* kContrast = "contrast";
inline constexpr const char* kSaturation = "saturation";
inline constexpr const char* kGrain = "grain";
inline constexpr const char* kAberration = "aberration";
inline constexpr const char* kMotionBlur = "motionBlur";
inline constexpr const char* kGradeTime = kTime;  // the tone mapping shader names its clock the same

// The most point lights that light a surface at once: MAX_POINT_LIGHTS in the shaders, defined
// for every shader when it is compiled (see shader_source.hpp).
inline constexpr int kMaxPointLights = 16;

}  // namespace redgem::shaders
