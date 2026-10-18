#pragma once

// The custom properties a scene's objects carry (glTF node extras, set in Blender or in the
// editor) and the words some of them take: the one contract between whoever makes a scene and
// the parts of the engine that read it.

namespace redgem::props {

// --- The properties of any object -------------------------------------------------
inline constexpr const char* kScript = "script";
inline constexpr const char* kBody = "body";
inline constexpr const char* kCollider = "collider";
inline constexpr const char* kTrigger = "trigger";
inline constexpr const char* kTag = "tag";
inline constexpr const char* kMass = "mass";
inline constexpr const char* kVisible = "visible";
inline constexpr const char* kWheel = "wheel";
// A terrain: a square grid mesh made in the editor, whose ground can be sculpted there.
inline constexpr const char* kTerrain = "terrain";
inline constexpr const char* kTerrainSize = "terrain_size";    // meters across
inline constexpr const char* kTerrainCells = "terrain_cells";  // quads per side
inline constexpr const char* kPrefab = "prefab";
inline constexpr const char* kTint = "tint";
// Levels of detail: an object is drawn only while its distance from the camera (meters) is at
// least `visible_from` and less than `visible_to`. Several versions of a model, each with its own
// range, make a detailed one up close and a light one far away.
inline constexpr const char* kVisibleFrom = "visible_from";
inline constexpr const char* kVisibleTo = "visible_to";
// False keeps an object out of the sun's shadows (a sky dome, a glow): it is still drawn.
inline constexpr const char* kCastShadow = "cast_shadow";
// Whether wind sways an object's foliage (its textured see-through materials). Not said, it does
// unless a skeleton moves the object (a character's hair follows its head, not the wind).
inline constexpr const char* kSway = "sway";

// --- The properties of a material (glTF material extras) --------------------------
// How a blended material shows what is behind it: mixed with it, sorted farthest first (the
// default), or `kDithered` (see MaterialDesc::dithered).
inline constexpr const char* kTransparency = "transparency";
inline constexpr const char* kDithered = "dithered";

// --- Physics: what `body` and `collider` may be set to ----------------------------
inline constexpr const char* kBodyStatic = "static";
inline constexpr const char* kBodyDynamic = "dynamic";
inline constexpr const char* kBodyKinematic = "kinematic";
inline constexpr const char* kBodyCharacter = "character";
inline constexpr const char* kBodyVehicle = "vehicle";
// Every body type, as lists show them. The first is what an object with a collider and no body is.
inline constexpr const char* kBodyTypes[] = {kBodyStatic, kBodyDynamic, kBodyKinematic, kBodyCharacter, kBodyVehicle};
inline constexpr const char* kColliderBox = "box";
inline constexpr const char* kColliderSphere = "sphere";
inline constexpr const char* kColliderCapsule = "capsule";
inline constexpr const char* kColliderMesh = "mesh";
// Every collider, as lists show them. The first is what a body without a collider gets.
inline constexpr const char* kColliders[] = {kColliderBox, kColliderSphere, kColliderCapsule, kColliderMesh};

// --- Environment: the look of a scene beyond its objects ---------------------------
// True on the object whose other properties describe the environment (see render/environment.hpp).
inline constexpr const char* kEnvironment = "environment";
namespace environment {
// Colors, as "#rrggbb".
inline constexpr const char* kSkyTop = "sky_top";
inline constexpr const char* kSkyHorizon = "sky_horizon";
inline constexpr const char* kGroundColor = "ground_color";
inline constexpr const char* kFogColor = "fog_color";
// Numbers.
inline constexpr const char* kFogDensity = "fog_density";
inline constexpr const char* kAmbient = "ambient";
inline constexpr const char* kExposure = "exposure";
inline constexpr const char* kBloom = "bloom";
inline constexpr const char* kVignette = "vignette";
inline constexpr const char* kSunGlow = "sun_glow";
inline constexpr const char* kWind = "wind";
inline constexpr const char* kShadows = "shadows";
inline constexpr const char* kAmbientOcclusion = "ambient_occlusion";
inline constexpr const char* kContrast = "contrast";
inline constexpr const char* kSaturation = "saturation";
inline constexpr const char* kGrain = "grain";
inline constexpr const char* kAberration = "aberration";
// The sky as a picture of the game's images folder, which also lights the scene.
inline constexpr const char* kSkyImage = "sky_image";        // its name
inline constexpr const char* kSkyRotation = "sky_rotation";  // degrees it is turned around the vertical
inline constexpr const char* kSkyIntensity = "sky_intensity";
// Quality.
inline constexpr const char* kMsaa = "msaa";  // samples per pixel of the scene's picture
inline constexpr const char* kShadowCascades = "shadow_cascades";
inline constexpr const char* kShadowDistance = "shadow_distance";  // meters
inline constexpr const char* kShadowSoftness = "shadow_softness";  // degrees across the sun
// How edges are smoothed: kTemporalAntialias, or after the picture is drawn (anything else).
inline constexpr const char* kAntialias = "antialias";
inline constexpr const char* kTemporalAntialias =
    "temporal";  // with the frames before (Environment::temporal_antialias)
// The brightest glowing particles together make a pixel (see Environment::glow_limit).
inline constexpr const char* kGlowLimit = "glow_limit";
}  // namespace environment

}  // namespace redgem::props
