#pragma once

// Import settings: how the engine takes in one asset of a game. They live next to the asset, in a
// sidecar file named after it ("models/tree.glb" -> "models/tree.glb.import.json"). An asset
// without a sidecar uses the defaults, and a sidecar may hold only some fields (the rest keep
// their defaults).
//
//   - a model (.glb, .gltf): its scale, its up axis, the colliders made for its meshes, and
//     whether its pictures are extracted into images/ (applied when the editor imports it);
//   - a texture (.png, .jpg, .jpeg, .webp, .ktx2): its color space and whether it is a normal map
//     (the Inspector warns when it goes into a material slot that reads it otherwise), its largest
//     side (applied when it is loaded or put on a material) and its mipmaps;
//   - a sound (.wav, .ogg, .mp3, .flac): streamed or loaded, looping, its volume, and whether a
//     positional play of it is mixed down to mono.

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace redgem::import_settings {

enum class Kind { Model, Texture, Sound };
enum class UpAxis { Y, Z };
enum class Colliders { None, Box, Mesh };
enum class Materials { Keep, Extract };
// Auto: the material slot decides (color slots sRGB, the others linear).
enum class ColorSpace { Auto, Srgb, Linear };
// Only None: the engine has no KTX2 encoder yet (a .ktx2 file is used as it is).
enum class Compression { None };

inline constexpr const char* kSidecarSuffix = ".import.json";
inline constexpr float kDefaultScale = 1.0f;
inline constexpr float kMinScale = 0.001f;
inline constexpr float kMaxScale = 1000.0f;
// max_size 0: the picture keeps its own size.
inline constexpr int kNoSizeLimit = 0;
inline constexpr int kMinTextureSize = 1;
inline constexpr int kMaxTextureSize = 16384;
inline constexpr float kDefaultVolume = 1.0f;
inline constexpr float kMinVolume = 0.0f;
inline constexpr float kMaxVolume = 1.0f;
// The most a sidecar file may hold.
inline constexpr std::size_t kMaxSidecarBytes = 64 * 1024;

struct ModelSettings {
  float scale = kDefaultScale;
  UpAxis up_axis = UpAxis::Y;
  Colliders colliders = Colliders::None;
  Materials materials = Materials::Keep;
  bool operator==(const ModelSettings& other) const;
};

struct TextureSettings {
  ColorSpace color_space = ColorSpace::Auto;
  bool normal_map = false;
  int max_size = kNoSizeLimit;
  // Smaller copies for a picture drawn on screen smaller than it is (scene textures always have them).
  bool mipmaps = false;
  Compression compression = Compression::None;
  bool operator==(const TextureSettings& other) const;
};

struct SoundSettings {
  bool stream = false;
  bool loop = false;
  float volume = kDefaultVolume;
  bool mono_3d = true;
  bool operator==(const SoundSettings& other) const;
};

using Settings = std::variant<ModelSettings, TextureSettings, SoundSettings>;

// An asset's kind, told by its extension (any case); nullopt for a file that has no settings.
std::optional<Kind> kind_of(const std::filesystem::path& asset);
Kind kind_of(const Settings& settings);
// "model", "texture" or "sound".
const char* kind_word(Kind kind);
std::filesystem::path sidecar_of(const std::filesystem::path& asset);
bool is_sidecar(const std::filesystem::path& file);
Settings defaults(Kind kind);

// The settings a sidecar's text gives; nullopt and `error` for text that is no JSON object, an
// unknown field, or a value of the wrong type or out of range.
std::optional<Settings> parse(Kind kind, const std::string& text, std::string& error);
// A sidecar's text: every field, as JSON.
std::string text_of(const Settings& settings);
// A field as text, for a list or a form: a choice field has its choices; true and false are a
// switch; anything else is a number.
struct Field {
  std::string name;
  std::string value;
  std::vector<std::string> choices;
};
// Every field, by name.
std::vector<Field> fields(const Settings& settings);
// Sets one field from text ("true", "0.5", "mesh"); false and `error` for an unknown field or a
// value it cannot take (the settings are then left as they were).
bool set(Settings& settings, const std::string& field, const std::string& value, std::string& error);

// A picture's size once its longest side is at most `max_size` (kNoSizeLimit: as it is), in
// proportion, each side at least 1.
struct PictureSize {
  int width = 0;
  int height = 0;
  bool operator==(const PictureSize& other) const { return width == other.width && height == other.height; }
};
PictureSize fitted_size(PictureSize size, int max_size);

// An asset's settings: the defaults without a sidecar; nullopt and `error` for a file of no kind
// or a sidecar that cannot be read or parsed. Reads through game_files (a built game's archive).
std::optional<Settings> read_checked(const std::filesystem::path& asset, std::string& error);
// The same, with a sidecar that cannot be used said in the log once per call and the defaults used.
ModelSettings model_settings(const std::filesystem::path& asset);
TextureSettings texture_settings(const std::filesystem::path& asset);
SoundSettings sound_settings(const std::filesystem::path& asset);
// Writes the sidecar (on the disk: a built game's files are read-only).
bool save(const std::filesystem::path& asset, const Settings& settings, std::string& error);

}  // namespace redgem::import_settings
