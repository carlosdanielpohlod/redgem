#include "app/game_project.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/config.hpp"
#include "editor/scene_document.hpp"
#include "editor/script_files.hpp"
#include "generated/templates.hpp"

namespace redgem {
namespace {

constexpr const char* kSettingsFile = "game.json";
constexpr const char* kGameFolder = "game";      // the game's files, next to a built executable
constexpr const char* kFallbackExecutable = "play";  // for a game whose name gives no usable file name
constexpr const char* kTemplateScriptsFolder = "/scripts/";
constexpr int kJsonIndent = 2;
constexpr const char* kPlatformerTemplate = "platformer";
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
constexpr Vec3 kGroundScale{3.0f, 1.0f, 3.0f};     // the plane primitive is 10 m: 30 m ground
constexpr Vec3 kCubePosition{0.0f, 0.5f, 0.0f};
constexpr Vec3 kHeroPosition{0.0f, 1.2f, 0.0f};
constexpr Vec3 kFollowCamera{0.0f, 6.0f, 9.0f};
constexpr Vec3 kCoinScale{0.6f, 0.08f, 0.6f};
constexpr float kGoldMetallic = 1.0f;
constexpr const char* kGamesFolderName = "RedGemGames";
constexpr const char* kConfigFolder = ".config/redgem";
constexpr const char* kRecentFileName = "recent.json";
constexpr std::size_t kMaxRecentGames = 12;
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
constexpr float kToneVolume = 0.35f;        // of full scale
constexpr float kToneFadeOut = 4.0f;        // how fast the tone dies away (higher = shorter tail)
constexpr float kFullScale16 = 32767.0f;
constexpr float kTwoPi = 6.28318530718f;
constexpr std::uint32_t kWavHeaderBytes = 36;  // bytes of a PCM WAV header after the RIFF size field
constexpr std::uint32_t kPcmFormatBytes = 16;
constexpr std::uint16_t kPcmFormat = 1;
constexpr std::uint16_t kMono = 1;
constexpr int kBitsPerByte = 8;

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
  const int sun = doc.add_light(LightType::Directional, "Sun", kSunColor, kSunIntensity);
  place(doc, sun, Vec3::zero(), Vec3::one(), quat_look_rotation(normalize(kSunDirection), Vec3::up()));
  const int camera = doc.add_camera("Camera", kCameraFov);
  place(doc, camera, camera_position, Vec3::one(), quat_look_rotation(camera_target - camera_position, Vec3::up()));
}

void build_empty(SceneDocument& doc) {
  const int ground = doc.add_primitive(PrimitiveShape::Plane, "Ground", kGroundColor);
  place(doc, ground, Vec3::zero(), kGroundScale);
  const int cube = doc.add_primitive(PrimitiveShape::Cube, "Cube", kAccentColor);
  place(doc, cube, kCubePosition);
  doc.set_prop(cube, config::kPropScript, std::string("spinner.rb"));
  add_sun_and_camera(doc, kEmptyCamera, kCubePosition);
}

void build_platformer(SceneDocument& doc) {
  const int ground = doc.add_primitive(PrimitiveShape::Plane, "Ground", kGroundColor);
  place(doc, ground, Vec3::zero(), kGroundScale);
  int index = 1;
  for (const Platform& platform : kPlatforms) {
    const int node = doc.add_primitive(PrimitiveShape::Cube, "Platform" + std::to_string(index++), kPlatformColor);
    place(doc, node, platform.position, platform.scale);
  }
  const int hero = doc.add_primitive(PrimitiveShape::Capsule, "Player", kHeroColor);
  place(doc, hero, kHeroPosition);
  doc.set_prop(hero, config::kPropBody, std::string("character"));
  doc.set_prop(hero, config::kPropScript, std::string("player.rb"));
  doc.set_prop(hero, config::kPropTag, std::string("player"));
  index = 1;
  for (const Vec3 position : kCoins) {
    const int coin = doc.add_primitive(PrimitiveShape::Cylinder, "Coin" + std::to_string(index++), kGoldColor);
    place(doc, coin, position, kCoinScale,
          quat_from_euler_degrees({kDegreesQuarter, 0.0f, 0.0f}));
    MaterialDesc gold = doc.material(doc.material_of(coin));
    gold.metallic = kGoldMetallic;
    doc.set_material(doc.material_of(coin), gold);
    doc.set_prop(coin, config::kPropTrigger, true);
    doc.set_prop(coin, config::kPropScript, std::string("coin.rb"));
    doc.set_prop(coin, config::kPropTag, std::string("coin"));
  }
  add_sun_and_camera(doc, kFollowCamera, kHeroPosition);
  const int camera = doc.desc().find_node("Camera");
  doc.set_prop(camera, config::kPropScript, std::string("follow_camera.rb"));
}

const std::map<std::string, std::function<void(SceneDocument&)>>& builders() {
  static const std::map<std::string, std::function<void(SceneDocument&)>> table = {
      {"empty", build_empty}, {kPlatformerTemplate, build_platformer}};
  return table;
}

// A mono 16-bit WAV with a tone gliding from one pitch to another while fading out.
bool write_tone(const std::filesystem::path& path, const Tone& tone, std::string& error) {
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
  const std::uint16_t block_align = kMono * kToneBitsPerSample / kBitsPerByte;
  const std::uint32_t data_bytes = sample_count * block_align;
  std::ofstream out(path, std::ios::binary);
  const auto put = [&out](const auto value) { out.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
  out.write("RIFF", 4);
  put(static_cast<std::uint32_t>(kWavHeaderBytes + data_bytes));
  out.write("WAVEfmt ", 8);
  put(kPcmFormatBytes);
  put(kPcmFormat);
  put(kMono);
  put(static_cast<std::uint32_t>(kToneSampleRate));
  put(static_cast<std::uint32_t>(kToneSampleRate * block_align));
  put(block_align);
  put(static_cast<std::uint16_t>(kToneBitsPerSample));
  out.write("data", 4);
  put(data_bytes);
  out.write(reinterpret_cast<const char*>(samples.data()), data_bytes);
  if (!out) error = "cannot write " + path.string();
  return static_cast<bool>(out);
}

bool is_empty_dir(const std::filesystem::path& dir) {
  return !std::filesystem::exists(dir) ||
         (std::filesystem::is_directory(dir) && std::filesystem::is_empty(dir));
}

}  // namespace

GameSettings read_game_settings(const std::filesystem::path& game_dir) {
  GameSettings settings;
  settings.title = game_dir.filename().string();
  settings.window_width = config::kWindowWidth;
  settings.window_height = config::kWindowHeight;
  std::ifstream in(game_dir / kSettingsFile);
  if (!in) return settings;
  const nlohmann::json json = nlohmann::json::parse(in, nullptr, false);
  if (json.is_discarded() || !json.is_object()) return settings;
  settings.title = json.value("title", settings.title);
  settings.window_width = json.value("window_width", settings.window_width);
  settings.window_height = json.value("window_height", settings.window_height);
  return settings;
}

bool write_game_settings(const std::filesystem::path& game_dir, const GameSettings& settings, std::string& error) {
  const nlohmann::json json = {{"title", settings.title},
                               {"window_width", settings.window_width},
                               {"window_height", settings.window_height}};
  return write_text_file(game_dir / kSettingsFile, json.dump(kJsonIndent) + "\n", error);
}

std::string template_description(const std::string& template_name) {
  if (template_name == "empty") return "Ground, sun, camera, a spinning cube";
  if (template_name == kPlatformerTemplate) return "Run, jump and collect coins";
  return "";
}

bool valid_game_name(const std::string& name) {
  if (name.empty() || name.size() > kMaxGameNameLength || name.front() == ' ') return false;
  return std::all_of(name.begin(), name.end(), [](unsigned char c) {
    return std::isalnum(c) != 0 || c == '_' || c == '-' || c == ' ';
  });
}

const std::vector<std::string>& game_templates() {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> out;
    for (const auto& [name, builder] : builders()) out.push_back(name);
    return out;
  }();
  return names;
}

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
  for (const char* folder : {config::kScriptsDirName, config::kSoundsDirName, config::kScenesDirName,
                             config::kModelsDirName}) {
    std::filesystem::create_directories(game_dir / folder);
  }
  const std::string prefix = template_name + kTemplateScriptsFolder;
  for (const auto& file : generated::kTemplateFiles) {
    const std::string path = file.path;
    if (path.rfind(prefix, 0) != 0) continue;
    if (!write_text_file(game_dir / config::kScriptsDirName / path.substr(prefix.size()), file.contents, error)) {
      return false;
    }
  }
  if (template_name == kPlatformerTemplate) {  // its scripts play these
    for (const Tone& tone : kPlatformerTones) {
      if (!write_tone(game_dir / config::kSoundsDirName / tone.file, tone, error)) return false;
    }
  }
  SceneDocument doc = SceneDocument::create_empty(game_dir / config::kSceneFileName);
  builder->second(doc);
  if (!doc.save(error)) return false;
  GameSettings settings = read_game_settings(game_dir);
  return write_game_settings(game_dir, settings, error);
}

std::filesystem::path normalized_folder(const std::filesystem::path& folder) {
  std::filesystem::path normal = folder.lexically_normal();
  // "a/b/" normalizes to a path whose last element is empty.
  if (!normal.has_filename() && normal.has_parent_path() && normal != normal.root_path()) normal = normal.parent_path();
  return normal;
}

bool build_game(const std::filesystem::path& game_folder, const std::filesystem::path& out_folder,
                const std::filesystem::path& engine_binary, std::string& error) {
  const std::filesystem::path game_dir = normalized_folder(game_folder);
  const std::filesystem::path out_dir = normalized_folder(out_folder);
  if (!std::filesystem::exists(game_dir / config::kSceneFileName)) {
    error = game_dir.string() + " is not a game (no " + config::kSceneFileName + ")";
    return false;
  }
  std::error_code fs_error;
  // Compare real locations: the output must not be the game or anywhere inside it.
  const std::filesystem::path game_real = std::filesystem::weakly_canonical(game_dir, fs_error);
  const std::filesystem::path out_real = std::filesystem::weakly_canonical(out_dir, fs_error);
  const std::filesystem::path relative = out_real.lexically_relative(game_real);
  if (!relative.empty() && *relative.begin() != "..") {
    error = "cannot build into " + out_dir.string() + ": it is inside the game folder";
    return false;
  }
  std::filesystem::create_directories(out_dir, fs_error);
  const std::filesystem::path binary = out_dir / executable_name(game_dir);
  std::filesystem::copy_file(engine_binary, binary, std::filesystem::copy_options::overwrite_existing, fs_error);
  if (fs_error) {
    error = "cannot copy the engine: " + fs_error.message();
    return false;
  }
  std::filesystem::permissions(binary, std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                                           std::filesystem::perms::others_exec,
                               std::filesystem::perm_options::add, fs_error);
  std::filesystem::remove_all(out_dir / kGameFolder, fs_error);
  std::filesystem::copy(game_dir, out_dir / kGameFolder, std::filesystem::copy_options::recursive, fs_error);
  if (fs_error) {
    error = "cannot copy the game: " + fs_error.message();
    return false;
  }
  return true;
}

std::string executable_name(const std::filesystem::path& game_dir) {
  std::string name;
  for (const char c : normalized_folder(game_dir).filename().string()) {
    const auto letter = static_cast<unsigned char>(c);
    name.push_back(std::isalnum(letter) != 0 ? static_cast<char>(std::tolower(letter)) : '_');
  }
  // It sits next to the game's files: it cannot have their folder's name.
  if (name.empty() || name == kGameFolder) return kFallbackExecutable;
  return name;
}

std::optional<std::filesystem::path> bundled_game(const std::filesystem::path& executable) {
  const std::filesystem::path game = executable.parent_path() / kGameFolder;
  std::error_code fs_error;
  if (!std::filesystem::exists(game / config::kSceneFileName, fs_error)) return std::nullopt;
  return game;
}

std::vector<std::filesystem::path> find_games(const std::filesystem::path& folder) {
  std::vector<std::filesystem::path> games;
  std::error_code fs_error;
  for (const auto& entry : std::filesystem::directory_iterator(folder, fs_error)) {
    // Unreadable folders (other users' temp dirs...) are simply skipped.
    if (entry.is_directory(fs_error) && std::filesystem::exists(entry.path() / config::kSceneFileName, fs_error)) {
      games.push_back(entry.path());
    }
  }
  std::sort(games.begin(), games.end());
  return games;
}

namespace {

std::filesystem::path home_dir() {
  const char* home = std::getenv("HOME");
  return home != nullptr ? std::filesystem::path(home) : std::filesystem::temp_directory_path();
}

}  // namespace

std::filesystem::path default_games_folder() { return home_dir() / kGamesFolderName; }

std::filesystem::path recent_games_file() {
  const char* config = std::getenv("XDG_CONFIG_HOME");
  const std::filesystem::path base = config != nullptr && *config != '\0'
                                         ? std::filesystem::path(config) / "redgem"
                                         : home_dir() / kConfigFolder;
  return base / kRecentFileName;
}

std::vector<std::filesystem::path> read_recent_games(const std::filesystem::path& file) {
  std::vector<std::filesystem::path> games;
  std::ifstream in(file);
  if (!in) return games;
  const nlohmann::json json = nlohmann::json::parse(in, nullptr, false);
  if (!json.is_array()) return games;
  std::error_code fs_error;
  for (const nlohmann::json& entry : json) {
    if (!entry.is_string()) continue;
    const std::filesystem::path game = entry.get<std::string>();
    if (std::filesystem::exists(game / config::kSceneFileName, fs_error)) games.push_back(game);
  }
  return games;
}

void remember_game(const std::filesystem::path& file, const std::filesystem::path& game_dir) {
  std::error_code fs_error;
  const std::filesystem::path game = std::filesystem::weakly_canonical(game_dir, fs_error);
  std::vector<std::filesystem::path> games = read_recent_games(file);
  games.erase(std::remove(games.begin(), games.end(), game), games.end());
  games.insert(games.begin(), game);
  if (games.size() > kMaxRecentGames) games.resize(kMaxRecentGames);
  nlohmann::json json = nlohmann::json::array();
  for (const auto& path : games) json.push_back(path.string());
  std::filesystem::create_directories(file.parent_path(), fs_error);
  std::string error;
  write_text_file(file, json.dump(kJsonIndent) + "\n", error);  // best effort: only a convenience
}

}  // namespace redgem
