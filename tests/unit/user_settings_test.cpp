#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "app/user_settings.hpp"
#include "core/platform.hpp"

using namespace redgem;

namespace {

constexpr int kWideWidth = 1280;
constexpr int kWideHeight = 720;
constexpr const char* kGpuVariable = "REDGEM_GPU";

// No REDGEM_GPU for the life of the object (an empty one says nothing), what it held put back.
class NoGpuVariable {
 public:
  NoGpuVariable() : before_(platform::environment(kGpuVariable)) { platform::set_environment(kGpuVariable, "", true); }
  ~NoGpuVariable() { platform::set_environment(kGpuVariable, before_, true); }
  NoGpuVariable(const NoGpuVariable&) = delete;
  NoGpuVariable& operator=(const NoGpuVariable&) = delete;

 private:
  std::string before_;
};

}  // namespace

TEST_CASE("the user's settings are a file of the user's RedGem folder") {
  CHECK(user_settings_file() == platform::user_data_dir() / "settings.json");
}

TEST_CASE("the command line beats the environment, which beats the saved setting") {
  UserSettings saved;
  saved.gpu = GpuChoice::Integrated;
  CHECK(resolve_gpu_choice(std::nullopt, std::nullopt, saved) == GpuChoice::Integrated);
  CHECK(resolve_gpu_choice(std::nullopt, std::string("dedicated"), saved) == GpuChoice::Dedicated);
  CHECK(resolve_gpu_choice(std::string("auto"), std::string("dedicated"), saved) == GpuChoice::Auto);
  // Values that are not choices are skipped, not treated as a choice.
  CHECK(resolve_gpu_choice(std::string("turbo"), std::string("dedicated"), saved) == GpuChoice::Dedicated);
  CHECK(resolve_gpu_choice(std::nullopt, std::string(""), saved) == GpuChoice::Integrated);
  CHECK(resolve_gpu_choice(std::nullopt, std::nullopt, UserSettings{}) == GpuChoice::Auto);
}

TEST_CASE("a run's card is the one its command line names, whatever else is set") {
  // (The environment and the saved file are the machine's own: only what beats them both is checked.)
  CHECK(gpu_choice_for_run(std::string("integrated")) == GpuChoice::Integrated);
  CHECK(gpu_choice_for_run(std::string("dedicated")) == GpuChoice::Dedicated);
}

TEST_CASE("settings are saved and read back; broken files give the defaults") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem_settings_test";
  std::filesystem::remove_all(dir);
  const std::filesystem::path file = dir / "nested" / "settings.json";
  CHECK(read_user_settings(file).gpu == GpuChoice::Auto);  // no file yet
  UserSettings settings;
  settings.gpu = GpuChoice::Integrated;
  std::string error;
  REQUIRE_MESSAGE(write_user_settings(file, settings, error), error);
  CHECK(read_user_settings(file).gpu == GpuChoice::Integrated);
  std::ofstream(file) << "{ not json";
  CHECK(read_user_settings(file).gpu == GpuChoice::Auto);
  std::ofstream(file) << R"({"gpu": "warp"})";
  CHECK(read_user_settings(file).gpu == GpuChoice::Auto);
  CHECK(user_settings_file().filename() == "settings.json");
  std::filesystem::remove_all(dir);
}

TEST_CASE("a game's display settings are kept in its own settings file, beside its player's card and language") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem_display_settings_test";
  std::filesystem::remove_all(dir);
  const std::filesystem::path file = dir / "settings.json";
  DisplaySettings defaults;
  defaults.resolution = {kWideWidth, kWideHeight};
  CHECK(read_game_display(file, defaults).resolution == defaults.resolution);  // no file yet
  DisplaySettings chosen = defaults;
  chosen.mode = WindowMode::Borderless;
  chosen.quality = QualityPreset::Low;
  std::string error;
  REQUIRE_MESSAGE(write_game_display(file, chosen, error), error);
  UserSettings user;
  user.gpu = GpuChoice::Integrated;
  REQUIRE_MESSAGE(write_user_settings(file, user, error), error);  // (the display stays)
  const DisplaySettings kept = read_game_display(file, defaults);
  CHECK(kept.mode == WindowMode::Borderless);
  CHECK(kept.quality == QualityPreset::Low);
  CHECK(read_user_settings(file).gpu == GpuChoice::Integrated);
  std::ofstream(file) << R"({"display": "broken"})";
  CHECK(read_game_display(file, defaults).mode == WindowMode::Windowed);
  REQUIRE_MESSAGE(write_game_display(file, chosen, error), error);
  CHECK(read_game_display(file, defaults).mode == WindowMode::Borderless);
  std::filesystem::remove_all(dir);
}

TEST_CASE("a built game's card and language come from its own settings file") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem_game_run_settings_test";
  std::filesystem::remove_all(dir);
  const std::filesystem::path file = dir / "settings.json";
  UserSettings chosen;
  chosen.gpu = GpuChoice::Integrated;
  chosen.language = "pt";
  std::string error;
  REQUIRE_MESSAGE(write_user_settings(file, chosen, error), error);
  CHECK(language_for_run(file) == "pt");
  CHECK(gpu_choice_for_run(std::string("dedicated"), file) == GpuChoice::Dedicated);  // the command line first
  {
    const NoGpuVariable no_variable;
    CHECK(gpu_choice_for_run(std::nullopt, file) == GpuChoice::Integrated);
  }
  std::filesystem::remove_all(dir);
}

namespace {

// What a file holds, read and closed (Windows removes no file that is open).
std::string text_of(const std::filesystem::path& file) {
  std::ifstream in(file);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

}  // namespace

TEST_CASE("a damaged settings file is moved aside, not written over, and the settings are written anew") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem_damaged_settings_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::filesystem::path file = dir / "settings.json";
  std::ofstream(file) << R"({"gpu": "integrated", "games": {)";  // cut short
  DisplaySettings chosen;
  chosen.quality = QualityPreset::High;
  std::string error;
  REQUIRE_MESSAGE(write_game_display(file, chosen, error), error);
  const std::filesystem::path aside = dir / "settings.json.damaged";
  REQUIRE(std::filesystem::exists(aside));
  CHECK(text_of(aside) == R"({"gpu": "integrated", "games": {)");
  CHECK(read_game_display(file, DisplaySettings{}).quality == QualityPreset::High);
  CHECK_FALSE(std::filesystem::exists(dir / "settings.json.part"));  // written whole, through a file beside it
  std::filesystem::remove_all(dir);
}
