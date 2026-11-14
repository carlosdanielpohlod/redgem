// GameObject natives for shape keys (glTF morph targets): the weights of the keys of an object's
// mesh and of the meshes under it (a model's parts), by name. See GameObject#morph in
// ruby/engine/game_object.rb.

#include <mruby.h>
#include <mruby/array.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

// The object and everything under it that is still there, parents first: a model and its parts.
std::vector<ObjectId> with_parts(const Scene& objects, ObjectId root) {
  std::vector<ObjectId> found{root};
  for (std::size_t i = 0; i < found.size(); ++i) {
    for (const ObjectId child : objects.at(found[i]).children) {
      if (objects.is_alive(child)) found.push_back(child);
    }
  }
  return found;
}

// Where an object keeps the weight of its shape key `name`; nullopt when its mesh has no such key.
std::optional<std::size_t> shape_key(const GameObject& object, const std::string& name) {
  const auto found = std::find(object.morph_names.begin(), object.morph_names.end(), name);
  const auto key = static_cast<std::size_t>(found - object.morph_names.begin());
  if (key >= object.morph_weights.size()) return std::nullopt;
  return key;
}

// Sets the weight of shape key `name` wherever the object or its parts have one: how many do.
mrb_value game_object_set_morph(mrb_state* mrb, mrb_value self) {
  char* name = nullptr;
  mrb_float weight = 0.0;
  mrb_get_args(mrb, "zf", &name, &weight);
  const auto value = static_cast<float>(finite(mrb, weight));
  Scene& objects = scene(mrb);
  mrb_int set = 0;
  for (const ObjectId id : with_parts(objects, alive_id(mrb, self))) {
    GameObject& part = objects.at(id);
    const std::optional<std::size_t> key = shape_key(part, name);
    if (!key) continue;
    part.morph_weights[*key] = value;
    ++set;
  }
  return mrb_int_value(mrb, set);
}

// The weight of shape key `name` on the first of the object and its parts that has it; nil when none has.
mrb_value game_object_morph_weight(mrb_state* mrb, mrb_value self) {
  const std::string name = name_arg(mrb);
  const Scene& objects = scene(mrb);
  for (const ObjectId id : with_parts(objects, alive_id(mrb, self))) {
    const GameObject& part = objects.at(id);
    if (const std::optional<std::size_t> key = shape_key(part, name))
      return mrb_float_value(mrb, part.morph_weights[*key]);
  }
  return mrb_nil_value();
}

// The names of the shape keys of the object and its parts, each once, in the order they come.
mrb_value game_object_morph_names(mrb_state* mrb, mrb_value self) {
  const Scene& objects = scene(mrb);
  std::vector<std::string> names;
  for (const ObjectId id : with_parts(objects, alive_id(mrb, self))) {
    for (const std::string& name : objects.at(id).morph_names) {
      if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
    }
  }
  mrb_value list = mrb_ary_new_capa(mrb, static_cast<mrb_int>(names.size()));
  for (const std::string& name : names) mrb_ary_push(mrb, list, mrb_symbol_value(mrb_intern_cstr(mrb, name.c_str())));
  return list;
}

}  // namespace

void install_shape_key_bindings(mrb_state* mrb) {
  RClass* game_object = mrb_class_get(mrb, kGameObjectClass);
  mrb_define_method(mrb, game_object, "__set_morph", game_object_set_morph, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, game_object, "__morph_weight", game_object_morph_weight, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, game_object, "morph_names", game_object_morph_names, MRB_ARGS_NONE());
}

}  // namespace redgem
