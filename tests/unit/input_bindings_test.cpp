// A game's own actions and axes, what each player binds them to, and the gamepads they hold.

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "input/input.hpp"
#include "input/input_config.hpp"

using namespace redgem;

namespace {

constexpr std::size_t kSecondPad = 1;
constexpr std::size_t kLastPad = kMaxGamepads - 1;
constexpr float kStickHalfway = 0.5f;

std::vector<InputBinding> listed(const ActionBinding* binding) {
  return binding == nullptr ? std::vector<InputBinding>{} : binding->all();
}

InputMap parsed(const char* text, std::vector<std::string>& problems) {
  return input_map_from_json(nlohmann::json::parse(text), problems);
}

}  // namespace

TEST_CASE("bindings are written as key: and pad: followed by the name") {
  CHECK(binding_from_text("key:space") == InputBinding(Key::Space));
  CHECK(binding_from_text("key:left_shift") == InputBinding(Key::LeftShift));
  CHECK(binding_from_text("pad:a") == InputBinding(GamepadButton::A));
  CHECK(binding_from_text("pad:dpad_left") == InputBinding(GamepadButton::DpadLeft));
  for (const char* wrong : {"space", "key:", "key:hyper", "pad:turbo", "mouse:left", "pad:space", ""}) {
    CHECK_FALSE(binding_from_text(wrong).has_value());
  }
  CHECK(binding_text(Key::Seven) == "key:7");
  CHECK(binding_text(GamepadButton::RightBumper) == "pad:right_bumper");
  CHECK(key_name(Key::Escape) == "escape");
  CHECK(gamepad_axis_name(GamepadAxis::LeftTrigger) == "left_trigger");
}

TEST_CASE("an action lists a binding once, keys before buttons") {
  ActionBinding binding;
  binding.add(GamepadButton::Y);
  binding.add(Key::K);
  binding.add(Key::K);
  CHECK(binding.all() == std::vector<InputBinding>{Key::K, GamepadButton::Y});
  binding.remove(Key::K);
  binding.remove(Key::J);  // (not there: nothing changes)
  CHECK(binding.all() == std::vector<InputBinding>{GamepadButton::Y});
  binding.remove(GamepadButton::Y);
  CHECK(binding.empty());
}

TEST_CASE("game.json's input section declares actions and axes") {
  std::vector<std::string> problems;
  const InputMap map = parsed(R"({"actions": {"dash": ["key:e", "pad:b"]},
      "axes": {"steer": {"negative": ["key:a", "pad:dpad_left"], "positive": ["key:d"], "pad_axis": "left_x"},
               "climb": {"pad_axis": "right_y", "invert": true}}})",
                              problems);
  CHECK(problems.empty());
  CHECK(listed(map.action("dash")) == std::vector<InputBinding>{Key::E, GamepadButton::B});
  const AxisBinding* steer = map.axis("steer");
  REQUIRE(steer != nullptr);
  CHECK(steer->negative.all() == std::vector<InputBinding>{Key::A, GamepadButton::DpadLeft});
  CHECK(steer->positive.all() == std::vector<InputBinding>{Key::D});
  CHECK(steer->stick == GamepadAxis::LeftX);
  CHECK_FALSE(steer->stick_inverted);
  const AxisBinding* climb = map.axis("climb");
  REQUIRE(climb != nullptr);
  CHECK(climb->negative.empty());  // a stick alone moves it
  CHECK(climb->stick_inverted);
}

TEST_CASE("what game.json's input section gets wrong is left out and said, the rest kept") {
  std::vector<std::string> problems;
  const InputMap map = parsed(R"({"actions": {"dash": ["key:e", "key:hyper", 7], "roll": "key:r"},
      "axes": {"steer": {"positive": ["key:d"], "pad_axis": "middle", "invert": "yes"}, "lean": 3}})",
                              problems);
  CHECK(listed(map.action("dash")) == std::vector<InputBinding>{Key::E});
  CHECK(map.action("roll") != nullptr);  // declared, bound to nothing
  CHECK(listed(map.action("roll")).empty());
  REQUIRE(map.axis("steer") != nullptr);
  CHECK_FALSE(map.axis("steer")->stick.has_value());
  CHECK_FALSE(map.axis("steer")->stick_inverted);
  CHECK(map.axis("lean") == nullptr);
  CHECK(problems.size() == 5);  // two dash bindings, roll, steer's stick, lean
  std::vector<std::string> none;
  CHECK(input_map_from_json(nlohmann::json(), none).empty());
  CHECK(input_map_from_json(nlohmann::json::parse(R"({"title": "no input"})"), none).empty());
  CHECK(none.empty());
}

TEST_CASE("an input section, or a part of it, that is not an object is said") {
  std::vector<std::string> problems;
  CHECK(parsed(R"({"actions": ["dash"], "axes": 3})", problems).empty());
  CHECK(problems.size() == 2);
  CHECK(parsed(R"(["dash"])", problems).empty());
  CHECK(problems.size() == 3);
}

TEST_CASE("a change that changes nothing keeps the player on the game's bindings") {
  PlayerBindings bindings;
  CHECK(bindings.unbind(0, "jump", InputBinding(Key::Z), std::nullopt));
  CHECK(bindings.bind(0, "jump", Key::Space, std::nullopt));
  CHECK(bindings.bind(0, "horizontal", Key::A, AxisSide::Negative));
  CHECK(bindings.changes(0).empty());
  bindings.declare_action("jump", {{Key::J}, {}});  // (a newer game binds it anew: the player follows)
  CHECK(listed(bindings.action(0, "jump")) == std::vector<InputBinding>{Key::J});
}

TEST_CASE("an input map written as JSON reads back the same") {
  InputMap map = InputMap::defaults();
  map.bind_action("dash", {{Key::E}, {GamepadButton::B, GamepadButton::Y}});
  map.bind_axis("climb", {{}, {{}, {GamepadButton::DpadUp}}, GamepadAxis::RightY, true});
  std::vector<std::string> problems;
  const InputMap back = input_map_from_json(input_map_json(map), problems);
  CHECK(problems.empty());
  for (const auto& [name, binding] : map.actions()) CHECK(listed(back.action(name)) == binding.all());
  for (const auto& [name, axis] : map.axes()) {
    const AxisBinding* read = back.axis(name);
    REQUIRE(read != nullptr);
    CHECK(read->negative.all() == axis.negative.all());
    CHECK(read->positive.all() == axis.positive.all());
    CHECK(read->stick == axis.stick);
    CHECK(read->stick_inverted == axis.stick_inverted);
  }
}

TEST_CASE("the five built-in names stay, and a game adds its own or binds them anew") {
  PlayerBindings bindings;
  for (const char* name : {"jump", "fire", "run"}) CHECK(bindings.is_action(name));
  for (const char* name : {"horizontal", "vertical"}) CHECK(bindings.is_axis(name));
  bindings.declare_action("dash", {{Key::E}, {}});
  bindings.declare_action("jump", {{Key::J}, {}});
  CHECK(listed(bindings.action(0, "dash")) == std::vector<InputBinding>{Key::E});
  CHECK(listed(bindings.action(0, "jump")) == std::vector<InputBinding>{Key::J});
  // A name is an action or an axis, never both.
  bindings.declare_axis("dash", {});
  CHECK_FALSE(bindings.is_action("dash"));
  CHECK(bindings.is_axis("dash"));
  CHECK(bindings.action(0, "fly") == nullptr);
  CHECK_FALSE(bindings.axis(0, "fly").has_value());
}

TEST_CASE("a player binds an action anew, takes bindings away and goes back to the game's") {
  PlayerBindings bindings;
  CHECK(bindings.bind(0, "jump", Key::K, std::nullopt));
  CHECK(listed(bindings.action(0, "jump")) == std::vector<InputBinding>{Key::Space, Key::K, GamepadButton::A});
  CHECK(bindings.unbind(0, "jump", InputBinding(Key::Space), std::nullopt));
  CHECK(listed(bindings.action(0, "jump")) == std::vector<InputBinding>{Key::K, GamepadButton::A});
  CHECK(bindings.unbind(0, "jump", std::nullopt, std::nullopt));
  CHECK(listed(bindings.action(0, "jump")).empty());
  CHECK(bindings.bind(0, "fire", Key::F, std::nullopt));
  bindings.reset(0, std::string("jump"));
  CHECK(listed(bindings.action(0, "jump")) == std::vector<InputBinding>{Key::Space, GamepadButton::A});
  CHECK(listed(bindings.action(0, "fire")).size() == 4);  // still changed
  bindings.reset(0, std::nullopt);
  CHECK(bindings.changes(0).empty());
}

TEST_CASE("a player binds each side of an axis; its stick stays") {
  PlayerBindings bindings;
  CHECK_FALSE(bindings.bind(0, "horizontal", Key::J, std::nullopt));  // which way, though?
  CHECK_FALSE(bindings.bind(0, "jump", Key::J, AxisSide::Negative));  // an action has no sides
  CHECK_FALSE(bindings.bind(0, "fly", Key::J, std::nullopt));
  CHECK(bindings.bind(0, "horizontal", Key::J, AxisSide::Negative));
  std::optional<AxisBinding> axis = bindings.axis(0, "horizontal");
  CHECK(axis->negative.all() == std::vector<InputBinding>{Key::A, Key::Left, Key::J});
  CHECK(axis->stick == GamepadAxis::LeftX);
  CHECK(bindings.unbind(0, "horizontal", InputBinding(Key::A), std::nullopt));  // from both sides
  CHECK(bindings.unbind(0, "horizontal", std::nullopt, AxisSide::Positive));
  axis = bindings.axis(0, "horizontal");
  CHECK(axis->negative.all() == std::vector<InputBinding>{Key::Left, Key::J});
  CHECK(axis->positive.empty());
  CHECK(axis->stick == GamepadAxis::LeftX);
  CHECK_FALSE(bindings.unbind(0, "jump", std::nullopt, AxisSide::Positive));
}

TEST_CASE("the players after the first start with the gamepad's bindings alone, and change their own") {
  PlayerBindings bindings;
  CHECK(listed(bindings.action(kSecondPad, "jump")) == std::vector<InputBinding>{GamepadButton::A});
  CHECK(bindings.axis(kLastPad, "vertical")->stick == GamepadAxis::LeftY);
  CHECK(bindings.axis(kLastPad, "vertical")->positive.empty());
  CHECK(bindings.bind(kSecondPad, "jump", Key::Up, std::nullopt));  // two players, one keyboard
  CHECK(listed(bindings.action(kSecondPad, "jump")) == std::vector<InputBinding>{Key::Up, GamepadButton::A});
  CHECK(listed(bindings.action(0, "jump")) == std::vector<InputBinding>{Key::Space, GamepadButton::A});
  CHECK(bindings.changes(0).empty());
}

TEST_CASE("a player's changes made before the game declares a name apply once it does") {
  PlayerBindings bindings;
  InputMap saved;
  saved.bind_action("dash", {{Key::Q}, {}});
  bindings.set_changes(0, saved);
  CHECK(bindings.action(0, "dash") == nullptr);  // not a name of the game yet
  bindings.declare_action("dash", {{Key::E}, {}});
  CHECK(listed(bindings.action(0, "dash")) == std::vector<InputBinding>{Key::Q});
}

TEST_CASE("one key bound to two actions triggers both") {
  ScriptedInputSource source;
  InputState input;
  input.bindings().declare_action("confirm", {{Key::Enter}, {}});
  source.set_key(Key::Enter, true);
  input.update(source);
  CHECK(input.action_down("fire") == true);
  CHECK(input.action_down("confirm") == true);
}

TEST_CASE("each player reads the gamepad of their own number") {
  ScriptedInputSource source;
  InputState input;
  source.set_gamepad_button(GamepadButton::A, true, kSecondPad);
  source.set_gamepad_axis(GamepadAxis::LeftX, kStickHalfway, kSecondPad);
  input.update(source);
  CHECK_FALSE(input.gamepad_connected());
  CHECK(input.gamepad_connected(kSecondPad));
  CHECK(input.connected_pads() == std::vector<std::size_t>{kSecondPad});
  CHECK(input.action_pressed("jump", kSecondPad) == true);
  CHECK(input.action_down("jump") == false);
  CHECK(input.axis("horizontal", kSecondPad) == doctest::Approx(kStickHalfway));
  CHECK(input.axis("horizontal") == doctest::Approx(0.0f));
  // The keyboard is the first player's.
  source.set_key(Key::Space, true);
  input.update(source);
  CHECK(input.action_down("jump") == true);
  CHECK(input.action_pressed("jump", kSecondPad) == false);
  CHECK(input.action_down("jump", kSecondPad) == true);  // still the second's own A
}

TEST_CASE("an action is released when the last of its keys and buttons lets go") {
  ScriptedInputSource source;
  InputState input;
  source.set_key(Key::Space, true);
  source.set_gamepad_button(GamepadButton::A, true);
  input.update(source);
  source.set_key(Key::Space, false);
  input.update(source);
  CHECK(input.action_released("jump") == false);  // A still holds it
  source.set_gamepad_button(GamepadButton::A, false);
  input.update(source);
  CHECK(input.action_released("jump") == true);
  input.update(source);
  CHECK(input.action_released("jump") == false);
  CHECK_FALSE(input.action_released("fly").has_value());
}

TEST_CASE("a rebound action follows its new key") {
  ScriptedInputSource source;
  InputState input;
  input.bindings().unbind(0, "jump", std::nullopt, std::nullopt);
  input.bindings().bind(0, "jump", Key::K, std::nullopt);
  source.set_key(Key::Space, true);
  input.update(source);
  CHECK(input.action_down("jump") == false);
  source.set_key(Key::K, true);
  input.update(source);
  CHECK(input.action_pressed("jump") == true);
}

TEST_CASE("a capture catches the next key or button that goes down, once") {
  ScriptedInputSource source;
  InputState input;
  source.set_key(Key::Enter, true);  // what opened the "press a key" screen, still held
  input.update(source);
  input.capture_next();
  CHECK(input.capturing());
  input.update(source);
  CHECK_FALSE(input.take_captured().has_value());
  source.set_gamepad_button(GamepadButton::Y, true, kSecondPad);
  input.update(source);
  const auto caught = input.take_captured();
  REQUIRE(caught.has_value());
  CHECK(caught->binding == InputBinding(GamepadButton::Y));
  CHECK(caught->pad == kSecondPad);
  CHECK_FALSE(input.capturing());
  CHECK_FALSE(input.take_captured().has_value());
  input.capture_next();
  source.set_key(Key::K, true);
  input.update(source);
  const auto key = input.take_captured();
  REQUIRE(key.has_value());
  CHECK(key->binding == InputBinding(Key::K));
  CHECK_FALSE(key->pad.has_value());
}

TEST_CASE("what a capture caught does not also read as pressed") {
  ScriptedInputSource source;
  InputState input;
  input.capture_next();
  source.set_key(Key::Space, true);
  source.set_gamepad_button(GamepadButton::A, true, kSecondPad);
  input.update(source);
  CHECK(input.take_captured()->binding == InputBinding(Key::Space));
  CHECK_FALSE(input.pressed(Key::Space));
  CHECK(input.action_pressed("jump") == false);
  CHECK(input.action_pressed("jump", kSecondPad) == false);  // (all that went down that frame is the capture's)
  CHECK(input.action_down("jump", kSecondPad) == true);
  input.capture_next();
  source.set_gamepad_button(GamepadButton::Y, true);
  input.update(source);
  CHECK(input.take_captured()->binding == InputBinding(GamepadButton::Y));
  CHECK_FALSE(input.gamepad_pressed(GamepadButton::Y));
  source.set_gamepad_button(GamepadButton::Y, false);
  input.update(source);
  CHECK(input.gamepad_released(GamepadButton::Y));
}

TEST_CASE("a capture called off catches nothing") {
  ScriptedInputSource source;
  InputState input;
  input.capture_next();
  input.cancel_capture();
  source.set_key(Key::K, true);
  input.update(source);
  CHECK_FALSE(input.take_captured().has_value());
}

TEST_CASE("every key and button that went down with the caught one is swallowed too") {
  ScriptedInputSource source;
  InputState input;
  input.capture_next();
  source.set_key(Key::K, true);
  source.set_key(Key::Space, true);
  source.set_gamepad_button(GamepadButton::Start, true, kLastPad);
  input.update(source);
  CHECK(input.take_captured()->binding == InputBinding(Key::K));
  CHECK_FALSE(input.pressed(Key::Space));
  CHECK_FALSE(input.gamepad_pressed(GamepadButton::Start, kLastPad));
  CHECK(input.action_pressed("jump") == false);
}

TEST_CASE("Escape, or a gamepad's B or Back, calls a capture off unless it is given others") {
  ScriptedInputSource source;
  InputState input;
  input.capture_next();
  source.set_key(Key::Escape, true);
  input.update(source);
  const auto cancelled = input.take_captured();
  REQUIRE(cancelled.has_value());
  CHECK(cancelled->cancelled);
  CHECK_FALSE(input.pressed(Key::Escape));  // (a pause menu does not open with it)
  CHECK_FALSE(input.capturing());
  input.capture_next();
  source.set_gamepad_button(GamepadButton::Back, true, kSecondPad);
  input.update(source);
  CHECK(input.take_captured()->cancelled);
  input.capture_next({Key::Q});  // a game that binds Escape too calls it off with Q
  source.release_all();
  input.update(source);
  source.set_key(Key::Escape, true);
  input.update(source);
  const auto escape = input.take_captured();
  REQUIRE(escape.has_value());
  CHECK_FALSE(escape->cancelled);
  CHECK(escape->binding == InputBinding(Key::Escape));
}

TEST_CASE("a player's change of an axis keeps the game's stick, which the game may change later") {
  PlayerBindings bindings;
  CHECK(bindings.bind(0, "vertical", Key::I, AxisSide::Positive));
  CHECK_FALSE(bindings.changes(0).axis("vertical")->stick.has_value());  // (not the player's to keep)
  AxisBinding inverted = *bindings.axis(0, "vertical");
  inverted.stick = GamepadAxis::RightY;
  inverted.stick_inverted = false;  // the game's "invert Y" option
  bindings.declare_axis("vertical", inverted);
  const auto now = bindings.axis(0, "vertical");
  CHECK(now->stick == GamepadAxis::RightY);
  CHECK_FALSE(now->stick_inverted);
  CHECK(now->positive.all() == std::vector<InputBinding>{Key::W, Key::Up, Key::I});
}

TEST_CASE("a name declared again as the other kind takes the players' changes of the old kind away") {
  PlayerBindings bindings;
  bindings.bind(0, "run", Key::R, std::nullopt);
  bindings.bind(kSecondPad, "run", Key::T, std::nullopt);
  bindings.declare_axis("run", {});
  CHECK(bindings.changes(0).action("run") == nullptr);
  CHECK(bindings.changes(kSecondPad).action("run") == nullptr);
  bindings.bind(0, "horizontal", Key::J, AxisSide::Negative);
  bindings.declare_action("horizontal", {{Key::H}, {}});
  CHECK(bindings.changes(0).axis("horizontal") == nullptr);
  CHECK(listed(bindings.action(0, "horizontal")) == std::vector<InputBinding>{Key::H});
}

TEST_CASE("a player's saved entry with anything wrong in it is dropped whole; a game's keeps the rest") {
  const char* text = R"({"actions": {"jump": ["key:k", "key:hyper"], "dash": ["key:q"], "roll": "key:r"},
      "axes": {"horizontal": {"negative": ["key:j", "pad:turbo"]}}})";
  std::vector<std::string> problems;
  const InputMap saved = input_map_from_json(nlohmann::json::parse(text), problems, BrokenEntries::Dropped);
  CHECK(saved.action("jump") == nullptr);
  CHECK(saved.action("roll") == nullptr);
  CHECK(saved.axis("horizontal") == nullptr);
  CHECK(listed(saved.action("dash")) == std::vector<InputBinding>{Key::Q});
  CHECK(problems.size() == 3);
  const InputMap declared = input_map_from_json(nlohmann::json::parse(text), problems);
  CHECK(listed(declared.action("jump")) == std::vector<InputBinding>{Key::K});
}
