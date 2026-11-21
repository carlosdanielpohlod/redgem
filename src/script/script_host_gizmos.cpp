// What the editor draws over a selected object while the game runs: the settings of the NPC parts
// the scripts gave it (ruby/engine/editor_gizmos.rb).

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>

#include "script/bindings/binding_helpers.hpp"
#include "script/npc_settings.hpp"
#include "script/script_host.hpp"

namespace redgem {
namespace {

constexpr const char* kGizmosModule = "EditorGizmos";
constexpr const char* kSettingsEntry = "__settings";

mrb_value entry(mrb_state* mrb, mrb_value hash, const char* key) {
  return mrb_hash_get(mrb, hash, mrb_symbol_value(binding::symbol(mrb, key)));
}

bool is_number(mrb_value value) { return mrb_float_p(value) || mrb_integer_p(value); }
bool is_text(mrb_value value) { return mrb_string_p(value); }
bool is_list(mrb_value value) { return mrb_array_p(value); }
bool is_hash(mrb_value value) { return mrb_hash_p(value); }

std::optional<float> number_at(mrb_state* mrb, mrb_value hash, const char* key) {
  const mrb_value value = entry(mrb, hash, key);
  if (mrb_float_p(value)) return static_cast<float>(mrb_float(value));
  if (mrb_integer_p(value)) return static_cast<float>(mrb_integer(value));
  return std::nullopt;
}

float number_or(mrb_state* mrb, mrb_value hash, const char* key, float fallback) {
  return number_at(mrb, hash, key).value_or(fallback);
}

std::optional<std::string> text_at(mrb_state* mrb, mrb_value hash, const char* key) {
  const mrb_value value = entry(mrb, hash, key);
  if (!is_text(value)) return std::nullopt;
  return std::string(RSTRING_PTR(value), static_cast<std::size_t>(RSTRING_LEN(value)));
}

std::vector<mrb_value> items_at(mrb_state* mrb, mrb_value hash, const char* key) {
  const mrb_value value = entry(mrb, hash, key);
  if (!is_list(value)) return {};
  std::vector<mrb_value> items;
  for (mrb_int i = 0; i < RARRAY_LEN(value); ++i) items.push_back(mrb_ary_ref(mrb, value, i));
  return items;
}

void read_interactable(mrb_state* mrb, mrb_value hash, NpcSettings& settings) {
  if (!is_number(entry(mrb, hash, "range"))) return;
  InteractableSettings part;
  part.range = number_or(mrb, hash, "range", kInteractableRange);
  part.prompt = text_at(mrb, hash, "prompt");
  settings.interactable = part;
}

void read_perception(mrb_state* mrb, mrb_value hash, NpcSettings& settings) {
  if (!is_number(entry(mrb, hash, "sight"))) return;
  PerceptionSettings part;
  part.sight = number_or(mrb, hash, "sight", kPerceptionSight);
  part.fov = number_or(mrb, hash, "fov", kPerceptionFov);
  part.eyes = number_or(mrb, hash, "eyes", kPerceptionEyes);
  part.hearing = number_or(mrb, hash, "hearing", kPerceptionHearing);
  settings.perception = part;
}

void read_schedule(mrb_state* mrb, mrb_value hash, NpcSettings& settings) {
  for (const mrb_value item : items_at(mrb, hash, "plan")) {
    if (mrb_string_p(item))
      settings.schedule.emplace_back(RSTRING_PTR(item), static_cast<std::size_t>(RSTRING_LEN(item)));
  }
}

void read_route(mrb_state* mrb, mrb_value hash, NpcSettings& settings) {
  const mrb_value destination = entry(mrb, hash, "destination");
  if (!binding::is_vector3(mrb, destination)) return;
  NavRoute route{binding::to_vec3(mrb, destination), {}};
  for (const mrb_value item : items_at(mrb, hash, "waypoints")) {
    if (binding::is_vector3(mrb, item)) route.waypoints.push_back(binding::to_vec3(mrb, item));
  }
  settings.route = route;
}

NpcSettings settings_from(mrb_state* mrb, mrb_value hash) {
  NpcSettings settings;
  if (!is_hash(hash)) return settings;
  read_interactable(mrb, hash, settings);
  read_perception(mrb, hash, settings);
  read_schedule(mrb, hash, settings);
  read_route(mrb, hash, settings);
  return settings;
}

}  // namespace

NpcSettings ScriptHost::npc_settings(ObjectId object) {
  if (!context_.scene->is_alive(object)) return {};
  mrb_state* mrb = vm_.state();
  const int arena = mrb_gc_arena_save(mrb);
  const RubyResult result = vm_.call(module_named(kGizmosModule), kSettingsEntry, {object_value(object)});
  // A part whose settings are wrong raises: the game reports that itself, the editor draws nothing.
  const NpcSettings settings = result.ok ? settings_from(mrb, result.value) : NpcSettings{};
  mrb_gc_arena_restore(mrb, arena);
  return settings;
}

}  // namespace redgem
