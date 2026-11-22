// Physics natives for lag compensation (Physics.rewind): the host's history of where the objects
// the network keeps in step were shown (app/lag_history.hpp).

#include <mruby.h>

#include <optional>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using binding::context;

// __lag_track(object): the host keeps where it was (a NetworkSync starting on it).
mrb_value physics_lag_track(mrb_state* mrb, mrb_value) {
  mrb_value object;
  mrb_get_args(mrb, "o", &object);
  const ObjectId id = binding::alive_id(mrb, object);
  if (context(mrb).lag.track) context(mrb).lag.track(id);
  return mrb_nil_value();
}

// __lag_shown(object, at): it was shown at host time `at` this frame.
mrb_value physics_lag_shown(mrb_state* mrb, mrb_value) {
  mrb_value object;
  mrb_float at = 0.0;
  mrb_get_args(mrb, "of", &object, &at);
  const ObjectId id = binding::alive_id(mrb, object);
  if (context(mrb).lag.shown) context(mrb).lag.shown(id, at);
  return mrb_nil_value();
}

// __rewind(time): puts what the host keeps where it was shown at `time`; the time it went to, or
// nil when it cannot (not hosting, or already rewound).
mrb_value physics_rewind(mrb_state* mrb, mrb_value) {
  mrb_float time = 0.0;
  mrb_get_args(mrb, "f", &time);
  const auto went = context(mrb).lag.rewind ? context(mrb).lag.rewind(time) : std::nullopt;
  return went ? mrb_float_value(mrb, *went) : mrb_nil_value();
}

// __restore: puts it all back.
mrb_value physics_restore(mrb_state* mrb, mrb_value) {
  if (context(mrb).lag.restore) context(mrb).lag.restore();
  return mrb_nil_value();
}

mrb_value physics_max_rewind(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).lag.longest); }

}  // namespace

void install_rewind_bindings(mrb_state* mrb) {
  RClass* physics_module = mrb_define_module(mrb, "Physics");
  mrb_define_module_function(mrb, physics_module, "__lag_track", physics_lag_track, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, physics_module, "__lag_shown", physics_lag_shown, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, physics_module, "__rewind", physics_rewind, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, physics_module, "__restore", physics_restore, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, physics_module, "max_rewind", physics_max_rewind, MRB_ARGS_NONE());
}

}  // namespace redgem
