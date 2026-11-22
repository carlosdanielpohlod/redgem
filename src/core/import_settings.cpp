#include "core/import_settings.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <nlohmann/json.hpp>

#include "core/files.hpp"
#include "core/game_files.hpp"
#include "core/log.hpp"

namespace redgem::import_settings {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

constexpr int kJsonIndent = 2;

namespace key {
constexpr const char* kScale = "scale";
constexpr const char* kUpAxis = "up_axis";
constexpr const char* kColliders = "colliders";
constexpr const char* kMaterials = "materials";
constexpr const char* kColorSpace = "color_space";
constexpr const char* kNormalMap = "normal_map";
constexpr const char* kMaxSize = "max_size";
constexpr const char* kMipmaps = "mipmaps";
constexpr const char* kCompression = "compression";
constexpr const char* kStream = "stream";
constexpr const char* kLoop = "loop";
constexpr const char* kVolume = "volume";
constexpr const char* kMono3d = "mono_3d";
}  // namespace key

template <typename Enum>
using Choice = std::pair<Enum, const char*>;

constexpr Choice<UpAxis> kUpAxes[] = {{UpAxis::Y, "y"}, {UpAxis::Z, "z"}};
constexpr Choice<Colliders> kColliderChoices[] = {
    {Colliders::None, "none"}, {Colliders::Box, "box"}, {Colliders::Mesh, "mesh"}};
constexpr Choice<Materials> kMaterialChoices[] = {{Materials::Keep, "keep"}, {Materials::Extract, "extract"}};
constexpr Choice<ColorSpace> kColorSpaces[] = {
    {ColorSpace::Auto, "auto"}, {ColorSpace::Srgb, "srgb"}, {ColorSpace::Linear, "linear"}};
constexpr Choice<Compression> kCompressions[] = {{Compression::None, "none"}};

struct KindExtensions {
  Kind kind;
  std::vector<std::string> extensions;
};
const std::vector<KindExtensions>& kind_extensions() {
  static const std::vector<KindExtensions> known = {
      {Kind::Model, {".glb", ".gltf"}},
      {Kind::Texture, {".png", ".jpg", ".jpeg", ".webp", ".ktx2"}},
      {Kind::Sound, {".wav", ".ogg", ".mp3", ".flac"}},
  };
  return known;
}
constexpr Choice<Kind> kKindWords[] = {{Kind::Model, "model"}, {Kind::Texture, "texture"}, {Kind::Sound, "sound"}};

std::string lowered(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
  return text;
}

template <typename Enum, std::size_t kCount>
const char* word_of(const Choice<Enum> (&choices)[kCount], Enum value) {
  for (const auto& [known, word] : choices) {
    if (known == value) return word;
  }
  return choices[0].second;  // (every value has its word)
}

template <typename Enum, std::size_t kCount>
std::string words_of(const Choice<Enum> (&choices)[kCount]) {
  std::string words;
  for (const auto& choice : choices) words += (words.empty() ? "" : ", ") + std::string(choice.second);
  return words;
}

// --- Writing ---------------------------------------------------------------------------------

constexpr int kFloatDigits = std::numeric_limits<float>::digits10;

// A float as the shortest decimal that reads back as it (0.01, not 0.009999999776482582).
double written_number(float number) {
  std::ostringstream text;
  text << std::setprecision(kFloatDigits) << number;
  return std::stod(text.str());
}

std::string number_text(float number) { return json(written_number(number)).dump(); }

json json_of(const ModelSettings& model) {
  return {{key::kScale, written_number(model.scale)},
          {key::kUpAxis, word_of(kUpAxes, model.up_axis)},
          {key::kColliders, word_of(kColliderChoices, model.colliders)},
          {key::kMaterials, word_of(kMaterialChoices, model.materials)}};
}
json json_of(const TextureSettings& texture) {
  return {{key::kColorSpace, word_of(kColorSpaces, texture.color_space)},
          {key::kNormalMap, texture.normal_map},
          {key::kMaxSize, texture.max_size},
          {key::kMipmaps, texture.mipmaps},
          {key::kCompression, word_of(kCompressions, texture.compression)}};
}
json json_of(const SoundSettings& sound) {
  return {{key::kStream, sound.stream},
          {key::kLoop, sound.loop},
          {key::kVolume, written_number(sound.volume)},
          {key::kMono3d, sound.mono_3d}};
}
json json_of(const Settings& settings) {
  return std::visit([](const auto& kind) { return json_of(kind); }, settings);
}

// --- Reading: each reader leaves `out` as it is when the field is missing ----------------------

bool refuse(std::string& error, const std::string& field, const std::string& why) {
  error = "'" + field + "' " + why;
  return false;
}

bool read_bool(const json& given, const char* field, bool& out, std::string& error) {
  if (!given.contains(field)) return true;
  if (!given[field].is_boolean()) return refuse(error, field, "needs true or false");
  out = given[field].get<bool>();
  return true;
}

bool finite_number(const json& value) { return value.is_number() && std::isfinite(value.get<double>()); }

bool float_in(const json& value, std::pair<float, float> range) {
  return finite_number(value) && value.get<float>() >= range.first && value.get<float>() <= range.second;
}

bool read_float(const json& given, const char* field, std::pair<float, float> range, float& out, std::string& error) {
  if (!given.contains(field)) return true;
  const json& value = given[field];
  if (!float_in(value, range)) {
    return refuse(error, field, "needs a number from " + number_text(range.first) + " to " + number_text(range.second));
  }
  out = value.get<float>();
  return true;
}

bool whole_number(const json& value) {
  return finite_number(value) && std::trunc(value.get<double>()) == value.get<double>();
}

// max_size: 0 (no limit) or a side from kMinTextureSize to kMaxTextureSize.
bool size_fits(const json& value) {
  if (!whole_number(value)) return false;
  const double size = value.get<double>();
  return size == kNoSizeLimit || (size >= kMinTextureSize && size <= kMaxTextureSize);
}

bool read_size(const json& given, int& out, std::string& error) {
  if (!given.contains(key::kMaxSize)) return true;
  const json& value = given[key::kMaxSize];
  if (!size_fits(value))
    return refuse(error, key::kMaxSize, "needs 0 (no limit) or a size up to " + std::to_string(kMaxTextureSize));
  out = static_cast<int>(value.get<double>());
  return true;
}

template <typename Enum, std::size_t kCount>
bool read_choice(const json& given, const char* field, const Choice<Enum> (&choices)[kCount], Enum& out,
                 std::string& error) {
  if (!given.contains(field)) return true;
  const json& value = given[field];
  for (const auto& [known, word] : choices) {
    const bool chosen = value.is_string() && value.get<std::string>() == word;
    if (chosen) {
      out = known;
      return true;
    }
  }
  return refuse(error, field, "needs one of: " + words_of(choices));
}

bool read_into(const json& given, ModelSettings& model, std::string& error) {
  return read_float(given, key::kScale, {kMinScale, kMaxScale}, model.scale, error) &&
         read_choice(given, key::kUpAxis, kUpAxes, model.up_axis, error) &&
         read_choice(given, key::kColliders, kColliderChoices, model.colliders, error) &&
         read_choice(given, key::kMaterials, kMaterialChoices, model.materials, error);
}
bool read_into(const json& given, TextureSettings& texture, std::string& error) {
  return read_choice(given, key::kColorSpace, kColorSpaces, texture.color_space, error) &&
         read_bool(given, key::kNormalMap, texture.normal_map, error) && read_size(given, texture.max_size, error) &&
         read_bool(given, key::kMipmaps, texture.mipmaps, error) &&
         read_choice(given, key::kCompression, kCompressions, texture.compression, error);
}
bool read_into(const json& given, SoundSettings& sound, std::string& error) {
  return read_bool(given, key::kStream, sound.stream, error) && read_bool(given, key::kLoop, sound.loop, error) &&
         read_float(given, key::kVolume, {kMinVolume, kMaxVolume}, sound.volume, error) &&
         read_bool(given, key::kMono3d, sound.mono_3d, error);
}

// The first field `given` has that the kind does not; empty when there is none.
std::string unknown_field(const json& given, const json& known) {
  for (const auto& item : given.items()) {
    if (!known.contains(item.key())) return item.key();
  }
  return {};
}

std::string field_names(const json& known) {
  std::string names;
  for (const auto& item : known.items()) names += (names.empty() ? "" : ", ") + item.key();
  return names;
}

std::optional<Settings> from_json(Kind kind, const json& given, std::string& error) {
  Settings settings = defaults(kind);
  if (!given.is_object()) {
    error = "import settings need a JSON object";
    return std::nullopt;
  }
  const std::string unknown = unknown_field(given, json_of(settings));
  if (!unknown.empty()) {
    error = "unknown field '" + unknown + "' (a " + kind_word(kind) + " has " + field_names(json_of(settings)) + ")";
    return std::nullopt;
  }
  const bool read = std::visit([&](auto& fields) { return read_into(given, fields, error); }, settings);
  return read ? std::optional<Settings>(settings) : std::nullopt;
}

// A value typed on a command line: JSON when it is a number, true, false or a quoted text; the
// text as it is otherwise ("mesh").
json typed_value(const std::string& value) {
  json parsed = json::parse(value, nullptr, false);
  const bool simple = !parsed.is_discarded() && parsed.is_primitive() && !parsed.is_null();
  return simple ? parsed : json(value);
}

template <typename Enum, std::size_t kCount>
std::vector<std::string> word_list(const Choice<Enum> (&choices)[kCount]) {
  std::vector<std::string> words;
  for (const auto& choice : choices) words.emplace_back(choice.second);
  return words;
}

// The choices of each field that has some.
const std::map<std::string, std::vector<std::string>>& field_choices() {
  static const std::map<std::string, std::vector<std::string>> known = {
      {key::kUpAxis, word_list(kUpAxes)},
      {key::kColliders, word_list(kColliderChoices)},
      {key::kMaterials, word_list(kMaterialChoices)},
      {key::kColorSpace, word_list(kColorSpaces)},
      {key::kCompression, word_list(kCompressions)},
  };
  return known;
}

std::vector<std::string> choices_of(const std::string& field) {
  const auto found = field_choices().find(field);
  return found == field_choices().end() ? std::vector<std::string>{} : found->second;
}

std::string shown(const json& value) { return value.is_string() ? value.get<std::string>() : value.dump(); }

template <typename Fields>
Fields read_or_defaults(const fs::path& asset) {
  std::string error;
  const auto settings = read_checked(asset, error);
  const Fields* fields = settings ? std::get_if<Fields>(&*settings) : nullptr;
  if (fields != nullptr) return *fields;
  if (!error.empty()) log::warning(sidecar_of(asset).string() + ": " + error + "; using the default import settings");
  return Fields{};
}

std::optional<std::string> sidecar_text(const fs::path& sidecar, std::string& error) {
  const auto bytes = game_files::read(sidecar, kMaxSidecarBytes);
  if (!bytes) {
    error = "cannot read the import settings (at most " + std::to_string(kMaxSidecarBytes) + " bytes)";
    return std::nullopt;
  }
  return std::string(bytes->begin(), bytes->end());
}

}  // namespace

bool ModelSettings::operator==(const ModelSettings& other) const {
  return scale == other.scale && up_axis == other.up_axis && colliders == other.colliders &&
         materials == other.materials;
}
bool TextureSettings::operator==(const TextureSettings& other) const {
  return color_space == other.color_space && normal_map == other.normal_map && max_size == other.max_size &&
         mipmaps == other.mipmaps && compression == other.compression;
}
bool SoundSettings::operator==(const SoundSettings& other) const {
  return stream == other.stream && loop == other.loop && volume == other.volume && mono_3d == other.mono_3d;
}

std::optional<Kind> kind_of(const fs::path& asset) {
  const std::string extension = lowered(asset.extension().string());
  for (const KindExtensions& known : kind_extensions()) {
    const auto& list = known.extensions;
    if (std::find(list.begin(), list.end(), extension) != list.end()) return known.kind;
  }
  return std::nullopt;
}

Kind kind_of(const Settings& settings) { return static_cast<Kind>(settings.index()); }

const char* kind_word(Kind kind) { return word_of(kKindWords, kind); }

fs::path sidecar_of(const fs::path& asset) { return fs::path(asset.string() + kSidecarSuffix); }

bool is_sidecar(const fs::path& file) {
  const std::string name = lowered(file.filename().string());
  const std::string suffix = kSidecarSuffix;
  return name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
}

Settings defaults(Kind kind) {
  switch (kind) {
    case Kind::Texture:
      return TextureSettings{};
    case Kind::Sound:
      return SoundSettings{};
    case Kind::Model:
      break;
  }
  return ModelSettings{};
}

std::optional<Settings> parse(Kind kind, const std::string& text, std::string& error) {
  const json given = json::parse(text, nullptr, false);
  if (given.is_discarded()) {
    error = "import settings are not valid JSON";
    return std::nullopt;
  }
  return from_json(kind, given, error);
}

std::string text_of(const Settings& settings) { return json_of(settings).dump(kJsonIndent) + "\n"; }

std::vector<Field> fields(const Settings& settings) {
  std::vector<Field> listed;
  const json written = json_of(settings);  // (kept: items() only points into it)
  for (const auto& item : written.items()) {
    listed.push_back({item.key(), shown(item.value()), choices_of(item.key())});
  }
  return listed;
}

bool set(Settings& settings, const std::string& field, const std::string& value, std::string& error) {
  json edited = json_of(settings);
  edited[field] = typed_value(value);
  auto changed = from_json(kind_of(settings), edited, error);
  if (changed) settings = *changed;
  return changed.has_value();
}

PictureSize fitted_size(PictureSize size, int max_size) {
  const int longest = std::max(size.width, size.height);
  const bool fits = max_size == kNoSizeLimit || longest <= max_size;
  if (fits) return size;
  const double scale = static_cast<double>(max_size) / longest;
  const auto side = [scale](int length) { return std::max(1, static_cast<int>(std::lround(length * scale))); };
  return {side(size.width), side(size.height)};
}

std::optional<Settings> read_checked(const fs::path& asset, std::string& error) {
  const auto kind = kind_of(asset);
  if (!kind) {
    error = asset.filename().string() + " has no import settings (it is no model, texture or sound)";
    return std::nullopt;
  }
  const fs::path sidecar = sidecar_of(asset);
  if (!game_files::is_file(sidecar)) return defaults(*kind);
  const auto text = sidecar_text(sidecar, error);
  return text ? parse(*kind, *text, error) : std::nullopt;
}

ModelSettings model_settings(const fs::path& asset) { return read_or_defaults<ModelSettings>(asset); }
TextureSettings texture_settings(const fs::path& asset) { return read_or_defaults<TextureSettings>(asset); }
SoundSettings sound_settings(const fs::path& asset) { return read_or_defaults<SoundSettings>(asset); }

bool save(const fs::path& asset, const Settings& settings, std::string& error) {
  return write_text_file(sidecar_of(asset), text_of(settings), error);
}

}  // namespace redgem::import_settings
