// Settings natives: how the player chose the game to be shown and heard (window mode, size,
// render scale, vsync, frame cap, quality, volume), read and changed as one hash of texts
// (see display_json). ruby/engine/settings.rb gives each setting its own name.

#include <mruby.h>
#include <mruby/array.h>

#include <iterator>
#include <string>

#include "core/log.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/message_values.hpp"

namespace redgem {
namespace {

using namespace binding;

mrb_value settings_read(mrb_state* mrb, mrb_value) {
  return value_from(mrb, display_json(context(mrb).display), HashKeys::Strings);
}

// Settings.__write(hash): what the hash says takes the place of what is in use, at once.
mrb_value settings_write(mrb_state* mrb, mrb_value self) {
  mrb_value changes;
  mrb_get_args(mrb, "H", &changes);
  ScriptContext& game = context(mrb);
  game.display = display_from_json(json_from(mrb, changes, kSaveHolder), game.display);
  if (game.apply_display) game.apply_display(game.display);
  return settings_read(mrb, self);
}

mrb_value size_pair(mrb_state* mrb, SurfaceSize size) {
  const mrb_value sides[] = {mrb_fixnum_value(size.width), mrb_fixnum_value(size.height)};
  return mrb_ary_new_from_values(mrb, static_cast<mrb_int>(std::size(sides)), sides);
}

mrb_value settings_modes(mrb_state* mrb, mrb_value) {
  const ScriptContext& game = context(mrb);
  const mrb_value modes = mrb_ary_new(mrb);
  if (!game.display_modes) return modes;
  for (const SurfaceSize size : game.display_modes()) mrb_ary_push(mrb, modes, size_pair(mrb, size));
  return modes;
}

// Settings.save: true once what is in use is kept for the game's next runs.
mrb_value settings_save(mrb_state* mrb, mrb_value) {
  const ScriptContext& game = context(mrb);
  if (!game.save_display) return mrb_false_value();
  std::string error;
  const bool saved = game.save_display(game.display, error);
  if (!saved) log::warning("could not keep the settings: " + error);
  return mrb_bool_value(saved);
}

}  // namespace

void install_settings_bindings(mrb_state* mrb) {
  RClass* settings = mrb_define_module(mrb, "Settings");
  mrb_define_module_function(mrb, settings, "__read", settings_read, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, settings, "__write", settings_write, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, settings, "__modes", settings_modes, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, settings, "save", settings_save, MRB_ARGS_NONE());
}

}  // namespace redgem
