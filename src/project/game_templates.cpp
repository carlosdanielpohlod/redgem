#include "project/game_templates.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <string_view>

#include "app/game_settings.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "editor/scene_document.hpp"
#include "generated/templates.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

constexpr const char* kTemplateScriptsFolder = "/scripts/";
constexpr const char* kEmptyTemplate = "empty";
constexpr const char* kPlatformerTemplate = "platformer";
// The objects of the templates, the scripts they run and what the scripts know them by.
constexpr const char* kGroundObject = "Ground";
constexpr const char* kSunObject = "Sun";
constexpr const char* kCameraObject = "Camera";
constexpr const char* kCubeObject = "Cube";
constexpr const char* kHeroObject = "Player";
constexpr const char* kPlatformObjects = "Platform";  // Platform1, Platform2, ...
constexpr const char* kCoinObjects = "Coin";
constexpr const char* kMenuObject = "Menu";  // carries the template's game_menu.rb
constexpr const char* kMenuTitleProp = "title";
constexpr const char* kSpinnerScript = "spinner.rb";
constexpr const char* kHeroScript = "player.rb";
constexpr const char* kCoinScript = "coin.rb";
constexpr const char* kCameraScript = "follow_camera.rb";
constexpr const char* kMenuScript = "game_menu.rb";
constexpr const char* kHeroTag = "player";
constexpr const char* kCoinTag = "coin";
constexpr float kDegreesQuarter = 90.0f;
// Colors (linear) and layout of the templates.
constexpr Color kGroundColor{0.32f, 0.36f, 0.34f, 1.0f};
constexpr Color kAccentColor{0.95f, 0.45f, 0.1f, 1.0f};
constexpr Color kPlatformColor{0.15f, 0.35f, 0.75f, 1.0f};
constexpr Color kHeroColor{0.9f, 0.3f, 0.1f, 1.0f};
constexpr Color kGoldColor{1.0f, 0.75f, 0.1f, 1.0f};
constexpr Color kSunColor{1.0f, 0.96f, 0.88f, 1.0f};
constexpr float kSunIntensity = 3.0f;
constexpr Vec3 kSunDirection{-0.4f, -1.0f, -0.6f};
constexpr float kCameraFov = 0.87f;  // ~50 degrees
constexpr Vec3 kEmptyCamera{0.0f, 4.0f, 8.0f};
constexpr Vec3 kGroundScale{3.0f, 1.0f, 3.0f};  // the plane primitive is 10 m: 30 m ground
constexpr Vec3 kCubePosition{0.0f, 0.5f, 0.0f};
constexpr Vec3 kHeroPosition{0.0f, 1.2f, 0.0f};
constexpr Vec3 kFollowCamera{0.0f, 6.0f, 9.0f};
constexpr Vec3 kCoinScale{0.6f, 0.08f, 0.6f};
constexpr float kGoldMetallic = 1.0f;
constexpr std::size_t kMaxGameNameLength = 64;

// Placeholder sound effects of the platformer template: short tones, generated so the template
// plays sounds without shipping audio files. (name, start Hz, end Hz, seconds)
struct Tone {
  const char* file;
  float start_hz;
  float end_hz;
  float seconds;
};
constexpr Tone kPlatformerTones[] = {{"jump.wav", 320.0f, 640.0f, 0.16f}, {"coin.wav", 990.0f, 1480.0f, 0.22f}};
constexpr int kToneSampleRate = 22050;
constexpr int kToneBitsPerSample = 16;
constexpr float kToneVolume = 0.35f;  // of full scale
constexpr float kToneFadeOut = 4.0f;  // how fast the tone dies away (higher = shorter tail)
constexpr float kFullScale16 = 32767.0f;
constexpr float kTwoPi = 6.28318530718f;
constexpr std::uint32_t kWavHeaderBytes = 36;  // bytes of a PCM WAV header after the RIFF size field
constexpr std::uint32_t kPcmFormatBytes = 16;
constexpr std::uint16_t kPcmFormat = 1;
constexpr std::uint16_t kMono = 1;
constexpr int kBitsPerByte = 8;
// The marks of a WAV file's parts.
constexpr std::string_view kRiffMark = "RIFF";
constexpr std::string_view kWaveFormatMark = "WAVEfmt ";
constexpr std::string_view kDataMark = "data";

struct Platform {
  Vec3 position;
  Vec3 scale;
};
constexpr Platform kPlatforms[] = {
    {{0.0f, 0.5f, -6.0f}, {3.0f, 1.0f, 3.0f}},
    {{3.0f, 1.25f, -10.0f}, {3.0f, 1.0f, 3.0f}},
    {{0.0f, 2.0f, -14.0f}, {4.0f, 1.0f, 4.0f}},
};
constexpr Vec3 kCoins[] = {{0.0f, 1.0f, -3.0f}, {0.0f, 2.0f, -6.0f}, {3.0f, 2.75f, -10.0f}, {0.0f, 3.5f, -14.0f}};

void place(SceneDocument& doc, int node, Vec3 position, Vec3 scale = Vec3::one(), Quat rotation = Quat::identity()) {
  Transform local = doc.node(node).local;
  local.position = position;
  local.scale = scale;
  local.rotation = rotation;
  doc.set_local(node, local);
}

void add_sun_and_camera(SceneDocument& doc, Vec3 camera_position, Vec3 camera_target) {
  const int sun = doc.add_light(LightType::Directional, kSunObject, kSunColor, kSunIntensity);
  place(doc, sun, Vec3::zero(), Vec3::one(), quat_look_rotation(normalize(kSunDirection), Vec3::up()));
  const int camera = doc.add_camera(kCameraObject, kCameraFov);
  place(doc, camera, camera_position, Vec3::one(), quat_look_rotation(camera_target - camera_position, Vec3::up()));
}

void add_ground(SceneDocument& doc) {
  const int ground = doc.add_primitive(PrimitiveShape::Plane, kGroundObject, kGroundColor);
  place(doc, ground, Vec3::zero(), kGroundScale);
}

void build_empty(SceneDocument& doc) {
  add_ground(doc);
  const int cube = doc.add_primitive(PrimitiveShape::Cube, kCubeObject, kAccentColor);
  place(doc, cube, kCubePosition);
  doc.set_prop(cube, props::kScript, std::string(kSpinnerScript));
  add_sun_and_camera(doc, kEmptyCamera, kCubePosition);
}

void add_platforms(SceneDocument& doc) {
  int index = 1;
  for (const Platform& platform : kPlatforms) {
    const int node =
        doc.add_primitive(PrimitiveShape::Cube, kPlatformObjects + std::to_string(index++), kPlatformColor);
    place(doc, node, platform.position, platform.scale);
  }
}

void add_hero(SceneDocument& doc) {
  const int hero = doc.add_primitive(PrimitiveShape::Capsule, kHeroObject, kHeroColor);
  place(doc, hero, kHeroPosition);
  doc.set_prop(hero, props::kBody, std::string(props::kBodyCharacter));
  doc.set_prop(hero, props::kScript, std::string(kHeroScript));
  doc.set_prop(hero, props::kTag, std::string(kHeroTag));
}

void add_coin(SceneDocument& doc, int index, Vec3 position) {
  const int coin = doc.add_primitive(PrimitiveShape::Cylinder, kCoinObjects + std::to_string(index), kGoldColor);
  place(doc, coin, position, kCoinScale, quat_from_euler_degrees({kDegreesQuarter, 0.0f, 0.0f}));
  MaterialDesc gold = doc.material(doc.material_of(coin));
  gold.metallic = kGoldMetallic;
  doc.set_material(doc.material_of(coin), gold);
  doc.set_prop(coin, props::kTrigger, true);
  doc.set_prop(coin, props::kScript, std::string(kCoinScript));
  doc.set_prop(coin, props::kTag, std::string(kCoinTag));
}

void build_platformer(SceneDocument& doc) {
  add_ground(doc);
  add_platforms(doc);
  add_hero(doc);
  int index = 1;
  for (const Vec3 position : kCoins) add_coin(doc, index++, position);
  add_sun_and_camera(doc, kFollowCamera, kHeroPosition);
  const int camera = doc.desc().find_node(kCameraObject);
  doc.set_prop(camera, props::kScript, std::string(kCameraScript));
  // Title screen, pause and end screen: its title is filled in when the game is created.
  const int menu = doc.add_empty(kMenuObject);
  doc.set_prop(menu, props::kScript, std::string(kMenuScript));
}

const std::map<std::string, std::function<void(SceneDocument&)>>& builders() {
  static const std::map<std::string, std::function<void(SceneDocument&)>> table = {
      {kEmptyTemplate, build_empty}, {kPlatformerTemplate, build_platformer}};
  return table;
}

// A tone gliding from one pitch to another while fading out, as 16-bit samples.
std::vector<std::int16_t> tone_samples(const Tone& tone) {
  const auto sample_count = static_cast<std::uint32_t>(tone.seconds * kToneSampleRate);
  std::vector<std::int16_t> samples(sample_count);
  float phase = 0.0f;
  for (std::uint32_t i = 0; i < sample_count; ++i) {
    const float progress = static_cast<float>(i) / static_cast<float>(sample_count);
    const float pitch = tone.start_hz + (tone.end_hz - tone.start_hz) * progress;
    phase += kTwoPi * pitch / kToneSampleRate;
    const float loudness = kToneVolume * std::exp(-kToneFadeOut * progress) * (1.0f - progress);
    samples[i] = static_cast<std::int16_t>(std::sin(phase) * loudness * kFullScale16);
  }
  return samples;
}

void put_mark(std::ostream& out, std::string_view mark) {
  out.write(mark.data(), static_cast<std::streamsize>(mark.size()));
}

template <typename Number>
void put_number(std::ostream& out, Number value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

// The header of a mono 16-bit WAV file holding `data_bytes` of samples.
void put_wav_header(std::ostream& out, std::uint32_t data_bytes) {
  const std::uint16_t block_align = kMono * kToneBitsPerSample / kBitsPerByte;
  put_mark(out, kRiffMark);
  put_number(out, static_cast<std::uint32_t>(kWavHeaderBytes + data_bytes));
  put_mark(out, kWaveFormatMark);
  put_number(out, kPcmFormatBytes);
  put_number(out, kPcmFormat);
  put_number(out, kMono);
  put_number(out, static_cast<std::uint32_t>(kToneSampleRate));
  put_number(out, static_cast<std::uint32_t>(kToneSampleRate * block_align));
  put_number(out, block_align);
  put_number(out, static_cast<std::uint16_t>(kToneBitsPerSample));
  put_mark(out, kDataMark);
  put_number(out, data_bytes);
}

// A mono 16-bit WAV with a tone gliding from one pitch to another while fading out.
bool write_tone(const std::filesystem::path& path, const Tone& tone, std::string& error) {
  const std::vector<std::int16_t> samples = tone_samples(tone);
  const auto data_bytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
  std::ofstream out(path, std::ios::binary);
  put_wav_header(out, data_bytes);
  out.write(reinterpret_cast<const char*>(samples.data()), data_bytes);
  if (!out) error = "cannot write " + path.string();
  return static_cast<bool>(out);
}

bool is_empty_dir(const std::filesystem::path& dir) {
  return !std::filesystem::exists(dir) || (std::filesystem::is_directory(dir) && std::filesystem::is_empty(dir));
}

}  // namespace

std::string template_description(const std::string& template_name) {
  if (template_name == kEmptyTemplate) return "Ground, sun, camera, a spinning cube";
  if (template_name == kPlatformerTemplate) return "Run, jump and collect coins";
  return "";
}

bool valid_game_name(const std::string& name) {
  if (name.empty() || name.size() > kMaxGameNameLength || name.front() == ' ') return false;
  return std::all_of(name.begin(), name.end(),
                     [](unsigned char c) { return std::isalnum(c) != 0 || c == '_' || c == '-' || c == ' '; });
}

const std::vector<std::string>& game_templates() {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> out;
    for (const auto& [name, builder] : builders()) out.push_back(name);
    return out;
  }();
  return names;
}

namespace {

void create_game_folders(const std::filesystem::path& game_dir) {
  for (const char* folder : {config::kScriptsDirName, config::kSoundsDirName, config::kScenesDirName,
                             config::kModelsDirName, config::kImagesDirName}) {
    std::filesystem::create_directories(game_dir / folder);
  }
}

bool write_template_scripts(const std::filesystem::path& game_dir, const std::string& template_name,
                            std::string& error) {
  const std::string prefix = template_name + kTemplateScriptsFolder;
  for (const auto& file : generated::kTemplateFiles) {
    const std::string path = file.path;
    if (path.rfind(prefix, 0) != 0) continue;
    if (!write_text_file(game_dir / config::kScriptsDirName / path.substr(prefix.size()), file.contents, error)) {
      return false;
    }
  }
  return true;
}

bool write_template_sounds(const std::filesystem::path& game_dir, const std::string& template_name,
                           std::string& error) {
  if (template_name != kPlatformerTemplate) return true;  // its scripts play these
  return std::all_of(std::begin(kPlatformerTones), std::end(kPlatformerTones), [&](const Tone& tone) {
    return write_tone(game_dir / config::kSoundsDirName / tone.file, tone, error);
  });
}

bool write_template_scene(const std::filesystem::path& game_dir, const std::function<void(SceneDocument&)>& build,
                          std::string& error) {
  SceneDocument doc = SceneDocument::create_empty(game_dir / config::kSceneFileName);
  build(doc);
  if (const int menu = doc.desc().find_node(kMenuObject); menu != kNoIndex) {
    doc.set_prop(menu, kMenuTitleProp, read_game_settings(game_dir).title);
  }
  return doc.save(error);
}

}  // namespace

bool create_game(const std::filesystem::path& game_dir, const std::string& template_name, std::string& error) {
  const auto builder = builders().find(template_name);
  if (builder == builders().end()) {
    error = "unknown template '" + template_name + "'";
    return false;
  }
  if (!is_empty_dir(game_dir)) {
    error = game_dir.string() + " already exists and is not empty";
    return false;
  }
  create_game_folders(game_dir);
  return write_template_scripts(game_dir, template_name, error) &&
         write_template_sounds(game_dir, template_name, error) &&
         write_template_scene(game_dir, builder->second, error) &&
         write_game_settings(game_dir, read_game_settings(game_dir), error);
}

}  // namespace redgem
