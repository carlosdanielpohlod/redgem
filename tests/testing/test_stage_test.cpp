#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "app/game_display.hpp"
#include "testing/frame_pictures.hpp"
#include "testing/test_stage.hpp"

using namespace redgem;
using namespace redgem::testing;

namespace {

const std::filesystem::path kGame = std::filesystem::path(REDGEM_SOURCE_DIR) / "games" / "platformer";

SystemTestOptions headless_run() {
  SystemTestOptions options;
  options.game_dir = kGame;
  options.headless = true;
  return options;
}

}  // namespace

TEST_CASE("a headless test shows its game to nobody, and still gives it something to sound through") {
  SystemTestOptions options;
  options.headless = true;
  FramePictures pictures("");
  Stage stage(options, "", pictures);
  CHECK(stage.presenter() == nullptr);
  REQUIRE(stage.audio() != nullptr);
  CHECK_FALSE(stage.audio()->play_sound("nothing-of-that-name", 1.0f, 1.0f));
}

TEST_CASE("a run starts with the display settings its settings file kept, and keeps them there") {
  const std::filesystem::path file = std::filesystem::temp_directory_path() / "redgem-testing-stage-settings.json";
  std::filesystem::remove(file);
  FramePictures pictures("");
  const Stage fresh(headless_run(), file, pictures);
  CHECK(fresh.display().quality == QualityPreset::Ultra);  // (the platformer's game.json says nothing of it)
  CHECK(fresh.display_kept_in().file == file);
  DisplaySettings chosen = fresh.display();
  chosen.quality = QualityPreset::High;
  std::string error;
  REQUIRE(keep_display(fresh.display_kept_in(), chosen, error));
  const Stage again(headless_run(), file, pictures);
  CHECK(again.display().quality == QualityPreset::High);
  std::filesystem::remove(file);
}
