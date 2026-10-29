// Time, Debug and Game natives: the running game's clock, its log and its scenes.

#include <mruby.h>
#include <mruby/string.h>

#include "core/log.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

// --- Time ------------------------------------------------------------------------------------

mrb_value time_delta(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).time.delta); }
mrb_value time_fixed_delta(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).time.fixed_delta); }
mrb_value time_elapsed(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).time.elapsed); }
mrb_value time_frame(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, context(mrb).time.frame); }

mrb_value time_scale(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).time.scale); }
mrb_value time_set_scale(mrb_state* mrb, mrb_value) {
  const mrb_float scale = at_least(mrb, float_arg(mrb), 0.0, "Time.scale");
  context(mrb).time.scale = static_cast<float>(scale);
  return mrb_float_value(mrb, scale);
}

// --- Debug -------------------------------------------------------------------------------------

std::string message_arg(mrb_state* mrb) { return mrb_str_to_cstr(mrb, mrb_obj_as_string(mrb, single_arg(mrb))); }

mrb_value debug_log(mrb_state* mrb, mrb_value) {
  log::info(message_arg(mrb));
  return mrb_nil_value();
}

mrb_value debug_warn(mrb_state* mrb, mrb_value) {
  log::warning(message_arg(mrb));
  return mrb_nil_value();
}

mrb_value debug_error(mrb_state* mrb, mrb_value) {
  log::error(message_arg(mrb));
  return mrb_nil_value();
}

// --- Game ----------------------------------------------------------------------------------------

mrb_value game_quit(mrb_state* mrb, mrb_value) {
  context(mrb).quit_requested = true;
  return mrb_nil_value();
}

mrb_value game_stop_recording(mrb_state* mrb, mrb_value) {
  context(mrb).stop_recording_requested = true;
  return mrb_nil_value();
}

bool has_scene(const ScriptContext& ctx, const std::string& name) { return ctx.scene_exists && ctx.scene_exists(name); }

mrb_value game_load_scene(mrb_state* mrb, mrb_value) {
  const std::string name = name_arg(mrb);
  ScriptContext& ctx = context(mrb);
  if (!has_scene(ctx, name)) mrb_raisef(mrb, E_ARGUMENT_ERROR, "no scene named %s", name.c_str());
  ctx.loading_error.clear();
  ctx.requested_scene = name;
  ctx.scene_request_async = false;
  ctx.cancel_scene_load = false;
  return mrb_nil_value();
}

mrb_value game_scene_name(mrb_state* mrb, mrb_value) { return mrb_str_new_cstr(mrb, context(mrb).scene_name.c_str()); }

mrb_value game_title(mrb_state* mrb, mrb_value) { return text_value(mrb, context(mrb).game_title); }

// mruby-time already defines the Time class; the game clock lives on it as class methods.
void install_time_bindings(mrb_state* mrb) {
  RClass* time_class = mrb_define_class(mrb, "Time", mrb->object_class);
  mrb_define_class_method(mrb, time_class, "delta", time_delta, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "fixed_delta", time_fixed_delta, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "elapsed", time_elapsed, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "frame", time_frame, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "scale", time_scale, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, time_class, "scale=", time_set_scale, MRB_ARGS_REQ(1));
}

void install_debug_bindings(mrb_state* mrb) {
  RClass* debug_module = mrb_define_module(mrb, "Debug");
  mrb_define_module_function(mrb, debug_module, "log", debug_log, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, debug_module, "warn", debug_warn, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, debug_module, "error", debug_error, MRB_ARGS_REQ(1));
}

void install_game_bindings(mrb_state* mrb) {
  RClass* game_module = mrb_define_module(mrb, "Game");
  mrb_define_module_function(mrb, game_module, "quit", game_quit, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, game_module, "stop_recording", game_stop_recording, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, game_module, "load_scene", game_load_scene, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, game_module, "scene_name", game_scene_name, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, game_module, "title", game_title, MRB_ARGS_NONE());
}

}  // namespace

void install_runtime_bindings(mrb_state* mrb) {
  install_time_bindings(mrb);
  install_debug_bindings(mrb);
  install_profile_bindings(mrb);
  install_debug_draw_bindings(mrb);
  install_game_bindings(mrb);
  install_loading_bindings(mrb);
}

}  // namespace redgem
