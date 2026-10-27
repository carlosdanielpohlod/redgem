#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <utility>

#include "scene/animation/animation_player.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kDefinitionParts = 4;
constexpr mrb_int kPairParts = 2;
constexpr mrb_int kStateParts = 5;
constexpr mrb_int kTransitionParts = 6;
constexpr mrb_int kFirst = 0;
constexpr mrb_int kSecond = 1;
constexpr mrb_int kThird = 2;
constexpr mrb_int kFourth = 3;
constexpr mrb_int kFifth = 4;
constexpr mrb_int kSixth = 5;

AnimationPlayer& animations(mrb_state* mrb) {
  return required(mrb, context(mrb).animations, "animation graphs are only available while a game runs");
}

mrb_value list(mrb_state* mrb, mrb_value value) {
  if (mrb_type(value) != MRB_TT_ARRAY) mrb_raise(mrb, E_ARGUMENT_ERROR, "expected an animation graph array");
  return value;
}

mrb_value parts(mrb_state* mrb, mrb_value value, mrb_int size) {
  list(mrb, value);
  if (RARRAY_LEN(value) != size) mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid animation graph entry size");
  return value;
}

mrb_value at(mrb_value value, mrb_int index) { return mrb_ary_entry(value, index); }

float number(mrb_state* mrb, mrb_value value) { return static_cast<float>(finite(mrb, mrb_as_float(mrb, value))); }

bool flag(mrb_state* mrb, mrb_value value) {
  if (mrb_true_p(value)) return true;
  if (mrb_false_p(value)) return false;
  mrb_raise(mrb, E_ARGUMENT_ERROR, "expected true or false in an animation graph");
  return false;
}

AnimationParameter parameter_value(mrb_state* mrb, mrb_value value) {
  if (mrb_true_p(value)) return true;
  if (mrb_false_p(value)) return false;
  return number(mrb, value);
}

AnimationParameters parameters(mrb_state* mrb, mrb_value value) {
  AnimationParameters result;
  list(mrb, value);
  for (mrb_int index = 0; index < RARRAY_LEN(value); ++index) {
    const auto pair = parts(mrb, at(value, index), kPairParts);
    const auto name = to_name(mrb, at(pair, kFirst));
    if (!result.emplace(name, parameter_value(mrb, at(pair, kSecond))).second)
      mrb_raise(mrb, E_ARGUMENT_ERROR, "duplicate animation parameter");
  }
  return result;
}

AnimationBlendPoint point(mrb_state* mrb, mrb_value value) {
  parts(mrb, value, kPairParts);
  const auto coordinates = at(value, kFirst);
  const auto clip = to_name(mrb, at(value, kSecond));
  if (mrb_type(coordinates) != MRB_TT_ARRAY) return {number(mrb, coordinates), clip};
  parts(mrb, coordinates, kPairParts);
  return {number(mrb, at(coordinates, kFirst)), clip, number(mrb, at(coordinates, kSecond))};
}

std::vector<AnimationBlendPoint> points(mrb_state* mrb, mrb_value value) {
  std::vector<AnimationBlendPoint> result;
  list(mrb, value);
  for (mrb_int index = 0; index < RARRAY_LEN(value); ++index) result.push_back(point(mrb, at(value, index)));
  return result;
}

std::pair<std::string, std::string> axes(mrb_state* mrb, mrb_value value) {
  if (mrb_nil_p(value)) return {};
  if (mrb_type(value) != MRB_TT_ARRAY) return {to_name(mrb, value), {}};
  parts(mrb, value, kPairParts);
  return {to_name(mrb, at(value, kFirst)), to_name(mrb, at(value, kSecond))};
}

AnimationGraphState state(mrb_state* mrb, mrb_value value) {
  parts(mrb, value, kStateParts);
  const auto parameters = axes(mrb, at(value, kSecond));
  return {to_name(mrb, at(value, kFirst)), parameters.first,
          points(mrb, at(value, kThird)),  flag(mrb, at(value, kFourth)),
          number(mrb, at(value, kFifth)),  parameters.second};
}

AnimationComparison comparison(mrb_state* mrb, mrb_value value) {
  const auto name = to_name(mrb, value);
  if (name == "equal") return AnimationComparison::Equal;
  if (name == "above") return AnimationComparison::Above;
  if (name == "below") return AnimationComparison::Below;
  mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown animation comparison");
  return AnimationComparison::Equal;
}

AnimationGraphTransition transition(mrb_state* mrb, mrb_value value) {
  parts(mrb, value, kTransitionParts);
  return {to_name(mrb, at(value, kFirst)),     to_name(mrb, at(value, kSecond)),        to_name(mrb, at(value, kThird)),
          comparison(mrb, at(value, kFourth)), parameter_value(mrb, at(value, kFifth)), number(mrb, at(value, kSixth))};
}

template <typename Value, typename Read>
std::vector<Value> entries(mrb_state* mrb, mrb_value value, Read read) {
  std::vector<Value> result;
  list(mrb, value);
  for (mrb_int index = 0; index < RARRAY_LEN(value); ++index) result.push_back(read(mrb, at(value, index)));
  return result;
}

AnimationGraphDefinition definition(mrb_state* mrb, mrb_value value) {
  parts(mrb, value, kDefinitionParts);
  return {to_name(mrb, at(value, kFirst)), parameters(mrb, at(value, kSecond)),
          entries<AnimationGraphState>(mrb, at(value, kThird), state),
          entries<AnimationGraphTransition>(mrb, at(value, kFourth), transition)};
}

mrb_value use_graph(mrb_state* mrb, mrb_value self) {
  const auto graph = definition(mrb, single_arg(mrb));
  std::string error;
  if (!animations(mrb).use_graph(scene(mrb), alive_id(mrb, self), graph, error))
    mrb_raise(mrb, E_ARGUMENT_ERROR, error.c_str());
  return mrb_nil_value();
}

mrb_value set_parameter(mrb_state* mrb, mrb_value self) {
  mrb_value name, value;
  mrb_get_args(mrb, "oo", &name, &value);
  const auto parameter = parameter_value(mrb, value);
  if (!animations(mrb).set_parameter(scene(mrb), alive_id(mrb, self), to_name(mrb, name), parameter))
    mrb_raise(mrb, E_ARGUMENT_ERROR,
              "unknown animation graph parameter, wrong type, or synchronized speed out of range");
  return mrb_nil_value();
}

mrb_value graph_state(mrb_state* mrb, mrb_value self) {
  const auto name = animations(mrb).graph_state(scene(mrb), alive_id(mrb, self));
  return name ? mrb_str_new_cstr(mrb, name->c_str()) : mrb_nil_value();
}

}  // namespace

void install_animation_graph_bindings(mrb_state* mrb) {
  auto* object = mrb_class_get(mrb, kGameObjectClass);
  mrb_define_method(mrb, object, "__animation_graph_use", use_graph, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, object, "__animation_graph_set", set_parameter, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, object, "__animation_graph_state", graph_state, MRB_ARGS_NONE());
}

}  // namespace redgem
