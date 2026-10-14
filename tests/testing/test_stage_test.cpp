#include <doctest/doctest.h>

#include "testing/frame_pictures.hpp"
#include "testing/test_stage.hpp"

using namespace redgem;
using namespace redgem::testing;

TEST_CASE("a headless test shows its game to nobody, and still gives it something to sound through") {
  SystemTestOptions options;
  options.headless = true;
  FramePictures pictures("");
  Stage stage(options, pictures);
  CHECK(stage.presenter() == nullptr);
  REQUIRE(stage.audio() != nullptr);
  CHECK_FALSE(stage.audio()->play_sound("nothing-of-that-name", 1.0f, 1.0f));
}
