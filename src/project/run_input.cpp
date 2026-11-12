#include "project/run_input.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

#include <nlohmann/json.hpp>

#include "core/safe_path.hpp"
#include "core/translations.hpp"

namespace redgem::tools {
namespace {

using Json = nlohmann::json;
using Fields = std::initializer_list<const char*>;

constexpr const char* kFrameField = "frame";
constexpr const char* kKeysField = "keys";
constexpr const char* kMouseField = "mouse";
constexpr const char* kPadsField = "pads";
constexpr const char* kTouchField = "touch";
constexpr const char* kTapField = "tap";
constexpr const char* kXField = "x";
constexpr const char* kYField = "y";
constexpr const char* kNormalizedField = "normalized";
constexpr const char* kButtonsField = "buttons";
constexpr const char* kClickField = "click";
constexpr const char* kWheelField = "wheel";
constexpr const char* kPadField = "pad";
constexpr const char* kAxesField = "axes";

const Fields kEventFields = {kFrameField, kKeysField, kMouseField, kPadsField, kTouchField, kTapField};
const Fields kInputFields = {kKeysField, kMouseField, kPadsField, kTouchField, kTapField};
const Fields kPointerFields = {kMouseField, kTouchField, kTapField};  // all of them move the one pointer
const Fields kPointFields = {kXField, kYField, kNormalizedField};
const Fields kMouseFields = {kXField, kYField, kNormalizedField, kButtonsField, kClickField, kWheelField};
const Fields kPadFields = {kPadField, kButtonsField, kAxesField};

constexpr std::size_t kMostNames = 128;
constexpr double kMostPixel = 65536.0;
constexpr double kMostFraction = 1.0;
constexpr double kMostWheelNotches = 100.0;
constexpr double kStickLeast = -1.0;
constexpr double kTriggerLeast = 0.0;
constexpr double kAxisMost = 1.0;
constexpr double kLastPad = static_cast<double>(kMaxGamepads - 1);

template <typename T>
using NameReader = std::optional<T> (*)(const std::string&);

bool is_among(const std::string& field, Fields fields) {
  return std::any_of(fields.begin(), fields.end(), [&field](const char* name) { return field == name; });
}

std::size_t count_present(const Json& object, Fields fields) {
  return static_cast<std::size_t>(
      std::count_if(fields.begin(), fields.end(), [&object](const char* name) { return object.contains(name); }));
}

bool is_within(double number, double least, double most) {
  return std::isfinite(number) && number >= least && number <= most;
}

bool is_name_list(const Json& list) { return list.is_array() && list.size() <= kMostNames; }

bool is_pad_list(const Json& list) { return list.is_array() && list.size() <= kMaxGamepads; }

bool is_event(const Json& value) {
  return value.is_object() && value.contains(kFrameField) && count_present(value, kInputFields) > 0;
}

bool shares_pointer(const Json& value) { return count_present(value, kPointerFields) > 1; }

bool names_point(const Json& object) { return count_present(object, kPointFields) > 0; }

bool is_trigger(GamepadAxis axis) { return axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger; }

double axis_least(GamepadAxis axis) { return is_trigger(axis) ? kTriggerLeast : kStickLeast; }

std::optional<double> number_within(const Json& value, double least, double most) {
  if (!value.is_number()) return std::nullopt;
  const double number = value.get<double>();
  if (!is_within(number, least, most)) return std::nullopt;
  return number;
}

std::optional<double> whole_within(const Json& value, double least, double most) {
  const auto number = number_within(value, least, most);
  if (!number) return std::nullopt;
  if (std::floor(*number) != *number) return std::nullopt;
  return number;
}

std::string first_problem(std::initializer_list<std::string> problems) {
  for (const std::string& problem : problems) {
    if (!problem.empty()) return problem;
  }
  return {};
}

std::string check_fields(const Json& object, Fields fields) {
  for (const auto& item : object.items()) {
    if (!is_among(item.key(), fields)) return tr("project.run.unknown_input_field", {{"field", printable(item.key())}});
  }
  return {};
}

// Reads `field` of `object` with `read`, when it is there.
template <typename Read>
std::string read_optional(const Json& object, const char* field, Read read) {
  return object.contains(field) ? read(object.at(field)) : std::string{};
}

// `field` of `object`, or `fallback` when it has none.
Json field_or(const Json& object, const char* field, const Json& fallback) {
  return object.contains(field) ? object.at(field) : fallback;
}

std::string not_an_object(const char* field) { return tr("project.run.bad_object", {{"field", field}}); }

template <typename T>
std::string read_names(const Json& list, const char* field, NameReader<T> from_name, std::vector<T>& into) {
  if (!is_name_list(list)) return tr("project.run.bad_names", {{"field", field}});
  for (const auto& value : list) {
    const auto found = value.is_string() ? from_name(value.get<std::string>()) : std::nullopt;
    if (!found) return tr("project.run.unknown_name", {{"field", field}, {"name", printable(value.dump())}});
    into.push_back(*found);
  }
  return {};
}

std::optional<bool> flag_of(const Json& object, const char* field) {
  if (!object.contains(field)) return false;
  const Json& value = object.at(field);
  return value.is_boolean() ? std::optional<bool>(value.get<bool>()) : std::nullopt;
}

std::optional<float> coordinate(const Json& object, const char* field, bool normalized) {
  const auto number = number_within(field_or(object, field, Json()), 0.0, normalized ? kMostFraction : kMostPixel);
  if (!number) return std::nullopt;
  return static_cast<float>(*number);
}

std::string read_point(const Json& object, RunPoint& point) {
  const auto normalized = flag_of(object, kNormalizedField);
  if (!normalized) return tr("project.run.bad_normalized");
  const auto x = coordinate(object, kXField, *normalized);
  if (!x) return tr("project.run.bad_point");
  const auto y = coordinate(object, kYField, *normalized);
  if (!y) return tr("project.run.bad_point");
  point = {*x, *y, *normalized};
  return {};
}

std::string read_frame(const Json& value, long long frames, RunInput& event) {
  const auto frame = whole_within(value.at(kFrameField), 0.0, static_cast<double>(frames - 1));
  if (!frame) return tr("project.run.bad_input_frame");
  event.frame = static_cast<long long>(*frame);
  return {};
}

std::string read_mouse_point(const Json& value, RunMouse& mouse) {
  if (!names_point(value)) return {};
  RunPoint point;
  const std::string problem = read_point(value, point);
  mouse.at = point;
  return problem;
}

std::string read_wheel(const Json& value, RunMouse& mouse) {
  const auto notches = number_within(value, -kMostWheelNotches, kMostWheelNotches);
  if (!notches) return tr("project.run.bad_wheel");
  mouse.wheel = static_cast<float>(*notches);
  return {};
}

std::string read_mouse_buttons(const Json& value, RunMouse& mouse) {
  return first_problem({read_optional(value, kButtonsField,
                                      [&mouse](const Json& list) {
                                        return read_names(list, kButtonsField, mouse_button_from_name, mouse.buttons);
                                      }),
                        read_optional(value, kClickField, [&mouse](const Json& list) {
                          return read_names(list, kClickField, mouse_button_from_name, mouse.clicks);
                        })});
}

std::string read_mouse(const Json& value, RunInput& event) {
  if (!value.is_object()) return not_an_object(kMouseField);
  RunMouse mouse;
  const std::string problem = first_problem(
      {check_fields(value, kMouseFields), read_mouse_point(value, mouse), read_mouse_buttons(value, mouse),
       read_optional(value, kWheelField, [&mouse](const Json& wheel) { return read_wheel(wheel, mouse); })});
  event.mouse = std::move(mouse);
  return problem;
}

std::string read_axes(const Json& value, RunPad& pad) {
  if (!value.is_object()) return not_an_object(kAxesField);
  for (const auto& item : value.items()) {
    const auto axis = gamepad_axis_from_name(item.key());
    if (!axis) return tr("project.run.unknown_name", {{"field", kAxesField}, {"name", printable(item.key())}});
    const auto amount = number_within(item.value(), axis_least(*axis), kAxisMost);
    if (!amount) return tr("project.run.bad_axis", {{"axis", item.key()}});
    pad.axes.push_back({*axis, static_cast<float>(*amount)});
  }
  return {};
}

std::string read_pad_number(const Json& value, RunPad& pad) {
  const auto number = whole_within(field_or(value, kPadField, Json(0)), 0.0, kLastPad);
  if (!number) return tr("project.run.bad_pad");
  pad.pad = static_cast<std::size_t>(*number);
  return {};
}

std::string read_pad(const Json& value, RunPad& pad) {
  if (!value.is_object()) return not_an_object(kPadsField);
  return first_problem({check_fields(value, kPadFields), read_pad_number(value, pad),
                        read_optional(value, kButtonsField,
                                      [&pad](const Json& list) {
                                        return read_names(list, kButtonsField, gamepad_button_from_name, pad.buttons);
                                      }),
                        read_optional(value, kAxesField, [&pad](const Json& axes) { return read_axes(axes, pad); })});
}

bool has_pad(const std::vector<RunPad>& pads, std::size_t number) {
  return std::any_of(pads.begin(), pads.end(), [number](const RunPad& pad) { return pad.pad == number; });
}

std::string read_pads(const Json& value, RunInput& event) {
  if (!is_pad_list(value)) return tr("project.run.bad_pads");
  for (const auto& item : value) {
    RunPad pad;
    const std::string problem = read_pad(item, pad);
    if (!problem.empty()) return problem;
    if (has_pad(event.pads, pad.pad)) return tr("project.run.duplicate_pad", {{"pad", std::to_string(pad.pad)}});
    event.pads.push_back(std::move(pad));
  }
  return {};
}

std::string read_touch(const Json& value, bool tap, RunInput& event) {
  if (!value.is_object()) return not_an_object(tap ? kTapField : kTouchField);
  RunTouch touch;
  touch.tap = tap;
  const std::string problem = first_problem({check_fields(value, kPointFields), read_point(value, touch.at)});
  event.touch = touch;
  return problem;
}

std::string read_devices(const Json& value, RunInput& event) {
  return first_problem(
      {read_optional(value, kKeysField,
                     [&event](const Json& keys) { return read_names(keys, kKeysField, key_from_name, event.keys); }),
       read_optional(value, kMouseField, [&event](const Json& mouse) { return read_mouse(mouse, event); }),
       read_optional(value, kPadsField, [&event](const Json& pads) { return read_pads(pads, event); }),
       read_optional(value, kTouchField, [&event](const Json& touch) { return read_touch(touch, false, event); }),
       read_optional(value, kTapField, [&event](const Json& tap) { return read_touch(tap, true, event); })});
}

struct ScreenPoint {
  float x = 0.0f;
  float y = 0.0f;
};

ScreenPoint on_screen(const RunPoint& point, RunScreen screen) {
  if (!point.normalized) return {point.x, point.y};
  return {point.x * screen.width, point.y * screen.height};
}

void move_to(ScriptedInputSource& source, const RunPoint& point, RunScreen screen) {
  const ScreenPoint at = on_screen(point, screen);
  source.move_mouse(at.x, at.y);
}

void apply_mouse(ScriptedInputSource& source, const RunMouse& mouse, RunScreen screen) {
  if (mouse.at) move_to(source, *mouse.at, screen);
  for (const MouseButton button : mouse.buttons) source.set_mouse_button(button, true);
  for (const MouseButton button : mouse.clicks) source.set_mouse_button(button, true);
  source.set_mouse_wheel(mouse.wheel);
}

void apply_pad(ScriptedInputSource& source, const RunPad& pad) {
  source.plug_gamepad(pad.pad);
  for (const GamepadButton button : pad.buttons) source.set_gamepad_button(button, true, pad.pad);
  for (const RunPadAxis& axis : pad.axes) source.set_gamepad_axis(axis.axis, axis.value, pad.pad);
}

void apply_touch(ScriptedInputSource& source, const RunTouch& touch, RunScreen screen) {
  move_to(source, touch.at, screen);
  source.set_mouse_button(MouseButton::Left, true);
}

bool holds(const RunMouse& mouse, MouseButton button) {
  return std::find(mouse.buttons.begin(), mouse.buttons.end(), button) != mouse.buttons.end();
}

void end_clicks(ScriptedInputSource& source, const RunMouse& mouse) {
  for (const MouseButton button : mouse.clicks) {
    if (!holds(mouse, button)) source.set_mouse_button(button, false);
  }
}

bool is_tap(const RunInput& event) { return event.touch && event.touch->tap; }

}  // namespace

std::string read_run_input(const nlohmann::json& value, long long frames, RunInput& event) {
  if (!is_event(value)) return tr("project.run.event_fields");
  if (shares_pointer(value)) return tr("project.run.one_pointer");
  return first_problem(
      {check_fields(value, kEventFields), read_frame(value, frames, event), read_devices(value, event)});
}

void apply_run_input(ScriptedInputSource& source, const RunInput& event, RunScreen screen) {
  source.release_all();
  for (const Key key : event.keys) source.set_key(key, true);
  if (event.mouse) apply_mouse(source, *event.mouse, screen);
  for (const RunPad& pad : event.pads) apply_pad(source, pad);
  source.set_touch_pointer(event.touch.has_value());
  if (event.touch) apply_touch(source, *event.touch, screen);
}

void end_run_pulses(ScriptedInputSource& source, const RunInput& event) {
  source.set_mouse_wheel(0.0f);
  if (event.mouse) end_clicks(source, *event.mouse);
  if (is_tap(event)) source.set_mouse_button(MouseButton::Left, false);
}

}  // namespace redgem::tools
