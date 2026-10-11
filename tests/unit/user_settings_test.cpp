#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "app/user_settings.hpp"
#include "core/platform.hpp"

using namespace redgem;

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
