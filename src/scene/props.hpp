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
// The collision layer of its body, one of game.json's physics layers (physics/collision_layers.hpp).
inline constexpr const char* kLayer = "layer";
inline constexpr const char* kVisible = "visible";
inline constexpr const char* kWheel = "wheel";
// A terrain: a square grid mesh made in the editor, whose ground can be sculpted there.
inline constexpr const char* kTerrain = "terrain";
inline constexpr const char* kTerrainSize = "terrain_size";    // meters across
inline constexpr const char* kTerrainCells = "terrain_cells";  // quads per side
inline constexpr const char* kPrefab = "prefab";
// An empty placement resolves this GLB from the game folder when its scene loads.
inline constexpr const char* kPrefabFile = "prefab_file";
inline constexpr const char* kTint = "tint";
// Levels of detail: an object is drawn only while its distance from the camera (meters) is at
// least `visible_from` and less than `visible_to`. Several versions of a model, each with its own
// range, make a detailed one up close and a light one far away.
inline constexpr const char* kVisibleFrom = "visible_from";
inline constexpr const char* kVisibleTo = "visible_to";
// False keeps an object out of the shadows (a sky dome, a glow): it is still drawn. On a point
// or spot light, true makes it cast shadows (off unless said).
inline constexpr const char* kCastShadow = "cast_shadow";
// Whether wind sways an object's foliage (its textured see-through materials). Not said, it does
// unless a skeleton moves the object (a character's hair follows its head, not the wind).
inline constexpr const char* kSway = "sway";

// --- The properties of a material (glTF material extras) --------------------------
// How a blended material shows what is behind it: mixed with it, sorted farthest first (the
// default), or `kDithered` (see MaterialDesc::dithered).
inline constexpr const char* kTransparency = "transparency";
inline constexpr const char* kDithered = "dithered";
inline constexpr const char* kWater = "water";
inline constexpr const char* kWaveHeight = "wave_height";
inline constexpr const char* kWavelength = "wavelength";
inline constexpr const char* kWaveSpeed = "wave_speed";
inline constexpr const char* kAbsorptionRed = "absorption_red";
inline constexpr const char* kAbsorptionGreen = "absorption_green";
inline constexpr const char* kAbsorptionBlue = "absorption_blue";
inline constexpr const char* kFoamWidth = "foam_width";
inline constexpr const char* kRefraction = "refraction";

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

// --- Joints: what holds an object to another one, or to the world (the editor's Joint card) ---
// Its kind, one of kJointKinds (in the order of physics' JointKind).
inline constexpr const char* kJoint = "joint";
inline constexpr const char* kJointFixed = "fixed";
inline constexpr const char* kJointHinge = "hinge";
inline constexpr const char* kJointSlider = "slider";
inline constexpr const char* kJointPoint = "point";
inline constexpr const char* kJointDistance = "distance";
inline constexpr const char* kJointKinds[] = {kJointFixed, kJointHinge, kJointSlider, kJointPoint, kJointDistance};
// The stable id of the object at its other end; none: the world.
inline constexpr const char* kJointTo = "joint_to";
// Points in the object's own space (its scale stretches them): where it turns or is held, and for
// a distance joint the point it is kept from, in the other object's space (for the world, meters
// from the object along the world's axes). The axis a hinge turns around or a slider slides along,
// in the object's own space.
inline constexpr const char* kJointAnchor[] = {"joint_anchor_x", "joint_anchor_y", "joint_anchor_z"};
inline constexpr const char* kJointOtherAnchor[] = {"joint_other_anchor_x", "joint_other_anchor_y",
                                                    "joint_other_anchor_z"};
inline constexpr const char* kJointAxis[] = {"joint_axis_x", "joint_axis_y", "joint_axis_z"};
// Hinges, degrees from where it starts (low -180..0, high 0..180); sliders, meters. Both or none.
inline constexpr const char* kJointLow = "joint_low";
inline constexpr const char* kJointHigh = "joint_high";
// Hinges: degrees per second, and newton meters at most (0: no motor).
inline constexpr const char* kJointMotorSpeed = "joint_motor_speed";
inline constexpr const char* kJointMotorTorque = "joint_motor_torque";
// Distance joints: meters (max unsaid or 0: as far apart as the ends start), and a spring (hertz) with
// its damping (0 bounces on, 1 settles at once).
inline constexpr const char* kJointMin = "joint_min";
inline constexpr const char* kJointMax = "joint_max";
inline constexpr const char* kJointSpring = "joint_spring";
inline constexpr const char* kJointDamping = "joint_damping";
// Newtons (or newton meters) it takes to break it; 0: it never breaks.
inline constexpr const char* kJointBreakForce = "joint_break_force";
// Every property of a joint but its vectors' numbers, which kJointAnchor, kJointOtherAnchor and
// kJointAxis list.
inline constexpr const char* kJointScalars[] = {kJoint,           kJointTo,          kJointLow,       kJointHigh,
                                                kJointMotorSpeed, kJointMotorTorque, kJointMin,       kJointMax,
                                                kJointSpring,     kJointDamping,     kJointBreakForce};

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
// How many point and spot lights may cast shadows at once (those whose `cast_shadow` is on).
inline constexpr const char* kLocalShadows = "local_shadows";
// How edges are smoothed: kTemporalAntialias, or after the picture is drawn (anything else).
inline constexpr const char* kAntialias = "antialias";
inline constexpr const char* kTemporalAntialias =
    "temporal";  // with the frames before (Environment::temporal_antialias)
// The brightest glowing particles together make a pixel (see Environment::glow_limit).
inline constexpr const char* kGlowLimit = "glow_limit";
}  // namespace environment

}  // namespace redgem::props
