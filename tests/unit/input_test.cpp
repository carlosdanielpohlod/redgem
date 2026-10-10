#include <doctest/doctest.h>

#include "input/input.hpp"

using namespace redgem;

TEST_CASE("key names map to keys") {
  CHECK(key_from_name("a") == Key::A);
  CHECK(key_from_name("z") == Key::Z);
  CHECK(key_from_name("7") == Key::Seven);
  CHECK(key_from_name("space") == Key::Space);
  CHECK(key_from_name("left_shift") == Key::LeftShift);
  CHECK_FALSE(key_from_name("hyper").has_value());
}

TEST_CASE("pressed and released are edge triggered") {
  ScriptedInputSource source;
  InputState input;
  source.set_key(Key::Space, true);
  input.update(source);
  CHECK(input.down(Key::Space));
  CHECK(input.pressed(Key::Space));
  input.update(source);
  CHECK(input.down(Key::Space));
  CHECK_FALSE(input.pressed(Key::Space));
  source.set_key(Key::Space, false);
  input.update(source);
  CHECK(input.released(Key::Space));
  CHECK_FALSE(input.down(Key::Space));
}

TEST_CASE("axes combine their keys") {
  ScriptedInputSource source;
  InputState input;
  input.update(source);
  CHECK(input.axis("horizontal") == doctest::Approx(0.0f));
  source.set_key(Key::D, true);
  input.update(source);
  CHECK(input.axis("horizontal") == doctest::Approx(1.0f));
  source.set_key(Key::Left, true);
  input.update(source);
  CHECK(input.axis("horizontal") == doctest::Approx(0.0f));
  source.release_all();
  source.set_key(Key::W, true);
  input.update(source);
  CHECK(input.axis("vertical") == doctest::Approx(1.0f));
  CHECK_FALSE(input.axis("sideways").has_value());
}

TEST_CASE("actions") {
  ScriptedInputSource source;
  InputState input;
  source.set_key(Key::Space, true);
  input.update(source);
  CHECK(input.action_down("jump") == true);
  CHECK(input.action_pressed("jump") == true);
  CHECK_FALSE(input.action_down("dance").has_value());
}
