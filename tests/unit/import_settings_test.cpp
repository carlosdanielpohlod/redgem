// Import settings: the sidecar format (kinds, defaults, partial files, refusals), setting a field
// from text, the command line that shows and writes them, and what each kind's settings change
// (how a sound plays, which slot a picture suits, a picture's size, a model's placement).

#include <doctest/doctest.h>

#include <raylib.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "audio/sound_import.hpp"
#include "core/exit_codes.hpp"
#include "core/files.hpp"
#include "core/image_header.hpp"
#include "core/import_settings.hpp"
#include "core/math.hpp"
#include "editor/document/material_textures.hpp"
#include "editor/document/picture_fit.hpp"
#include "editor/model_import_settings.hpp"
#include "project/search_command.hpp"
#include "temp_dirs.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::import_settings;
namespace fs = std::filesystem;

namespace {

constexpr float kTolerance = 1e-4f;
constexpr int kBigSide = 64;
constexpr int kSmallSide = 16;

// A game folder of its own, gone with the test.
class TinyGame {
 public:
  explicit TinyGame(const std::string& name) : dir_(redgem::test::temp_root() / ("redgem_import_settings_" + name)) {
    fs::remove_all(dir_);
    write("scene.glb", "glb");
    write("models/tree.glb", "glb");
    write("sounds/hit.wav", "wav");
  }
  ~TinyGame() { fs::remove_all(dir_); }
  TinyGame(const TinyGame&) = delete;
  TinyGame& operator=(const TinyGame&) = delete;

  void write(const std::string& file, const std::string& text) const {
    fs::create_directories((dir_ / file).parent_path());
    std::ofstream(dir_ / file) << text;
  }
  const fs::path& dir() const { return dir_; }

 private:
  fs::path dir_;
};

std::string field_value(const Settings& settings, const std::string& name) {
  for (const Field& field : fields(settings)) {
    if (field.name == name) return field.value;
  }
  return {};
}

std::vector<std::uint8_t> png_of_side(int side) {
  Image image = GenImageColor(side, side / 2, ::Color{255, 0, 0, 255});
  int bytes = 0;
  unsigned char* png = ExportImageToMemory(image, ".png", &bytes);
  UnloadImage(image);
  std::vector<std::uint8_t> out(png, png + bytes);
  MemFree(png);
  return out;
}

}  // namespace

TEST_CASE("an asset's kind is told by its extension, and its sidecar is named after it") {
  CHECK(kind_of(fs::path("models/tree.GLB")) == Kind::Model);
  CHECK(kind_of(fs::path("images/bark.jpeg")) == Kind::Texture);
  CHECK(kind_of(fs::path("sounds/hit.ogg")) == Kind::Sound);
  CHECK_FALSE(kind_of(fs::path("scripts/tree.rb")).has_value());
  CHECK(sidecar_of(fs::path("models/tree.glb")) == fs::path("models/tree.glb.import.json"));
  CHECK(is_sidecar(fs::path("models/tree.glb.import.json")));
  CHECK_FALSE(is_sidecar(fs::path(".import.json")));
  CHECK(kind_of(defaults(Kind::Sound)) == Kind::Sound);
}

TEST_CASE("a sidecar may hold some fields: the others keep their defaults") {
  std::string error;
  const auto settings = parse(Kind::Model, R"({"scale": 0.5, "colliders": "mesh"})", error);
  REQUIRE_MESSAGE(settings.has_value(), error);
  const auto& model = std::get<ModelSettings>(*settings);
  CHECK(model.scale == doctest::Approx(0.5));
  CHECK(model.colliders == Colliders::Mesh);
  CHECK(model.up_axis == UpAxis::Y);
  CHECK(model.materials == Materials::Keep);
}

TEST_CASE("a sidecar with an unknown field, a wrong type or a value out of range is refused") {
  std::string error;
  CHECK_FALSE(parse(Kind::Sound, R"({"volume": 2})", error).has_value());
  CHECK(error.find("volume") != std::string::npos);
  CHECK_FALSE(parse(Kind::Sound, R"({"loudness": 1})", error).has_value());
  CHECK(error.find("loudness") != std::string::npos);
  CHECK_FALSE(parse(Kind::Model, R"({"up_axis": "x"})", error).has_value());
  CHECK(error.find("y, z") != std::string::npos);
  CHECK_FALSE(parse(Kind::Texture, R"({"max_size": 12.5})", error).has_value());
  CHECK_FALSE(parse(Kind::Texture, R"({"compression": "ktx2"})", error).has_value());
  CHECK_FALSE(parse(Kind::Texture, R"({"mipmaps": 1})", error).has_value());
  CHECK_FALSE(parse(Kind::Model, "[1, 2]", error).has_value());
  CHECK_FALSE(parse(Kind::Model, "{nope", error).has_value());
}

TEST_CASE("settings written as text read back the same") {
  TextureSettings texture;
  texture.color_space = ColorSpace::Linear;
  texture.normal_map = true;
  texture.max_size = 512;
  std::string error;
  const auto read = parse(Kind::Texture, text_of(texture), error);
  REQUIRE_MESSAGE(read.has_value(), error);
  CHECK(std::get<TextureSettings>(*read) == texture);
}

TEST_CASE("a field is set from text, and a value it cannot take leaves the settings as they were") {
  Settings settings = defaults(Kind::Sound);
  std::string error;
  CHECK(set(settings, "loop", "true", error));
  CHECK(set(settings, "volume", "0.25", error));
  CHECK(std::get<SoundSettings>(settings).loop);
  CHECK(std::get<SoundSettings>(settings).volume == doctest::Approx(0.25));
  CHECK_FALSE(set(settings, "volume", "loud", error));
  CHECK_FALSE(set(settings, "pitch", "1", error));
  CHECK(std::get<SoundSettings>(settings).volume == doctest::Approx(0.25));
  Settings model = defaults(Kind::Model);
  CHECK(set(model, "up_axis", "z", error));
  CHECK(field_value(model, "up_axis") == "z");
}

TEST_CASE("the fields of a form say their choices") {
  for (const Field& field : fields(defaults(Kind::Model))) {
    if (field.name == "colliders") CHECK(field.choices == std::vector<std::string>{"none", "box", "mesh"});
    if (field.name == "scale") CHECK(field.choices.empty());
  }
}

TEST_CASE("an asset without a sidecar uses the defaults, and a broken sidecar is said") {
  TinyGame game("read");
  std::string error;
  const auto none = read_checked(game.dir() / "models/tree.glb", error);
  REQUIRE(none.has_value());
  CHECK(std::get<ModelSettings>(*none) == ModelSettings{});
  game.write("models/tree.glb.import.json", R"({"scale": "big"})");
  CHECK_FALSE(read_checked(game.dir() / "models/tree.glb", error).has_value());
  CHECK(error.find("scale") != std::string::npos);
  CHECK(import_settings::model_settings(game.dir() / "models/tree.glb") == ModelSettings{});  // (said in the log)
  CHECK_FALSE(read_checked(game.dir() / "scene.json", error).has_value());
}

TEST_CASE("redgem assets --settings lists an asset's settings and writes the fields given") {
  TinyGame game("command");
  const std::string dir = game.dir().string();
  const auto shown = tools::run_assets_command({dir, "--settings", "sounds/hit.wav"});
  CHECK(shown.exit_code == kExitSuccess);
  CHECK(shown.out.find("sound sounds/hit.wav") != std::string::npos);
  CHECK(shown.out.find("  volume = 1.0") != std::string::npos);
  const auto written = tools::run_assets_command({dir, "--settings", "sounds/hit.wav", "loop=true", "volume=0.5"});
  CHECK(written.exit_code == kExitSuccess);
  CHECK(written.out.find("wrote sounds/hit.wav.import.json") != std::string::npos);
  const auto settings = sound_settings(game.dir() / "sounds/hit.wav");
  CHECK(settings.loop);
  CHECK(settings.volume == doctest::Approx(0.5));
}

TEST_CASE("redgem assets --settings refuses a missing file, a bad word and a value out of range") {
  TinyGame game("command_refusals");
  const std::string dir = game.dir().string();
  CHECK(tools::run_assets_command({dir, "--settings", "sounds/none.wav"}).exit_code != kExitSuccess);
  CHECK(tools::run_assets_command({dir, "--settings", "../outside.wav"}).exit_code != kExitSuccess);
  CHECK(tools::run_assets_command({dir, "--settings", "sounds/hit.wav", "loop"}).exit_code != kExitSuccess);
  const auto refused = tools::run_assets_command({dir, "--settings", "models/tree.glb", "scale=0"});
  CHECK(refused.exit_code != kExitSuccess);
  CHECK(refused.err.find("scale") != std::string::npos);
  CHECK_FALSE(fs::exists(game.dir() / "models/tree.glb.import.json"));
}

TEST_CASE("a sidecar is not listed as an asset") {
  TinyGame game("listing");
  game.write("models/tree.glb.import.json", "{}");
  const auto found = tools::run_assets_command({game.dir().string(), "tree"});
  CHECK(found.out.find("import.json") == std::string::npos);
  CHECK(found.out.find("models/tree.glb") != std::string::npos);
}

TEST_CASE("a sound's settings decide whether it streams, loops, how loud it is and whether it is mixed down") {
  SpatialSound positional;
  SoundSettings settings;
  VoicePlan plan = voice_plan(positional, settings);
  CHECK_FALSE(plan.stream);
  CHECK_FALSE(plan.loop);
  CHECK(plan.mono);
  settings.loop = true;
  settings.volume = 0.5f;
  settings.mono_3d = false;
  plan = voice_plan(positional, settings);
  CHECK(plan.loop);
  CHECK(plan.stream);  // (a loop always streams)
  CHECK(plan.gain == doctest::Approx(0.5));
  CHECK_FALSE(plan.mono);
  SpatialSound flat;
  flat.positional = false;
  CHECK_FALSE(voice_plan(flat, SoundSettings{}).mono);
  SoundSettings streamed;
  streamed.stream = true;
  plan = voice_plan(flat, streamed);
  CHECK(plan.stream);
  CHECK_FALSE(plan.loop);
}

TEST_CASE("a picture's settings say which material slots it suits") {
  using material_textures::Slot;
  using material_textures::SlotFit;
  TextureSettings plain;
  CHECK(material_textures::slot_fit(Slot::Normal, plain) == SlotFit::Fits);
  CHECK(material_textures::slot_fit(Slot::BaseColor, plain) == SlotFit::Fits);
  TextureSettings normal;
  normal.normal_map = true;
  CHECK(material_textures::slot_fit(Slot::BaseColor, normal) == SlotFit::NormalMapElsewhere);
  CHECK(material_textures::slot_fit(Slot::Normal, normal) == SlotFit::Fits);
  TextureSettings linear;
  linear.color_space = ColorSpace::Linear;
  CHECK(material_textures::slot_fit(Slot::Emissive, linear) == SlotFit::OtherColorSpace);
  CHECK(material_textures::slot_fit(Slot::Occlusion, linear) == SlotFit::Fits);
  TextureSettings srgb;
  srgb.color_space = ColorSpace::Srgb;
  CHECK(material_textures::slot_fit(Slot::MetallicRoughness, srgb) == SlotFit::OtherColorSpace);
}

TEST_CASE("a picture bigger than its max size is scaled down in proportion") {
  CHECK(fitted_size({2048, 1024}, 512) == PictureSize{512, 256});
  CHECK(fitted_size({100, 3000}, 300) == PictureSize{10, 300});
  CHECK(fitted_size({300, 200}, 512) == PictureSize{300, 200});
  CHECK(fitted_size({4096, 1}, 16) == PictureSize{16, 1});
  CHECK(fitted_size({4096, 4096}, kNoSizeLimit) == PictureSize{4096, 4096});
}

TEST_CASE("a picture put on a material is carried no bigger than its max size") {
  const std::vector<std::uint8_t> big = png_of_side(kBigSide);
  const auto same = picture_fit::fitted_picture(big, kNoSizeLimit);
  REQUIRE(same.has_value());
  CHECK(*same == big);
  const auto fitted = picture_fit::fitted_picture(big, kSmallSide);
  REQUIRE(fitted.has_value());
  const auto size = encoded_image_size(fitted->data(), fitted->size());
  REQUIRE(size.has_value());
  CHECK(size->width == static_cast<std::uint32_t>(kSmallSide));
  CHECK(size->height == static_cast<std::uint32_t>(kSmallSide / 2));
  CHECK_FALSE(picture_fit::fitted_picture({1, 2, 3}, kSmallSide).has_value());
}

TEST_CASE("a model's top-level nodes are scaled and turned so that its up axis is Y") {
  ModelSettings settings;
  settings.scale = 2.0f;
  settings.up_axis = UpAxis::Z;
  redgem::Transform local;
  local.position = {0.0f, 0.0f, 1.0f};
  const redgem::Transform placed = model_settings::settled(local, settings);
  CHECK(approx_equal(placed.position, Vec3{0.0f, 2.0f, 0.0f}, kTolerance));
  CHECK(approx_equal(placed.scale, Vec3{2.0f, 2.0f, 2.0f}, kTolerance));
  CHECK(approx_equal(rotate(placed.rotation, Vec3{0.0f, 0.0f, 1.0f}), Vec3{0.0f, 1.0f, 0.0f}, kTolerance));
  const redgem::Transform kept = model_settings::settled(local, ModelSettings{});
  CHECK(approx_equal(kept.position, local.position, kTolerance));
}
