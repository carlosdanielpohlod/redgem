#include <mruby.h>

#include "script/bindings/binding_helpers.hpp"
#include "core/safe_path.hpp"
#include "net/network_session.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {
using namespace binding;

mrb_value load_async(mrb_state* mrb, mrb_value) {
  const std::string name = name_arg(mrb);
  auto& ctx = context(mrb);
  if (!safe_asset_name(name)) mrb_raise(mrb, E_ARGUMENT_ERROR, "scene name must stay inside the game folder");
  ctx.loading_error.clear();
  ctx.requested_scene = name;
  ctx.scene_request_async = true;
  ctx.cancel_scene_load = false;
  return mrb_nil_value();
}

bool pending(const ScriptContext& ctx) { return ctx.scene_request_async && ctx.requested_scene.has_value(); }

mrb_value loading(mrb_state* mrb, mrb_value) {
  const auto& ctx = context(mrb);
  return mrb_bool_value(!ctx.loading_scene.empty() || pending(ctx));
}

mrb_value loading_scene(mrb_state* mrb, mrb_value) {
  const auto& ctx = context(mrb);
  const std::string name =
      ctx.scene_request_async ? ctx.requested_scene.value_or(ctx.loading_scene) : ctx.loading_scene;
  return name.empty() ? mrb_nil_value() : text_value(mrb, name);
}

mrb_value loading_error(mrb_state* mrb, mrb_value) {
  const auto& error = context(mrb).loading_error;
  return error.empty() ? mrb_nil_value() : text_value(mrb, error);
}

mrb_value cancel_loading(mrb_state* mrb, mrb_value) {
  auto& ctx = context(mrb);
  const bool queued = pending(ctx);
  const bool active = !ctx.loading_scene.empty();
  ctx.cancel_scene_load = true;
  if (queued) ctx.requested_scene.reset();
  return mrb_bool_value(active || queued);
}
// Game.__hold(held): the world stays frozen as while loading, until a network game's machines all
// have the next level.
mrb_value hold(mrb_state* mrb, mrb_value) {
  context(mrb).level_held = bool_arg(mrb);
  return mrb_nil_value();
}
// Game.__held?: whether the world is held for a network game's change of level.
mrb_value held(mrb_state* mrb, mrb_value) { return mrb_bool_value(context(mrb).level_held); }

// Game.__online?: whether this machine is in a network game, whose level the host changes for all.
// (Asked of the session itself: a script may have replaced the Network module.)
mrb_value online(mrb_state* mrb, mrb_value) {
  const net::NetworkSession* network = context(mrb).network;
  return mrb_bool_value(network != nullptr && network->state() == net::NetworkSession::State::Online);
}
}  // namespace

void install_loading_bindings(mrb_state* mrb) {
  auto* game = mrb_module_get(mrb, "Game");
  mrb_define_module_function(mrb, game, "__load_scene_async", load_async, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, game, "__hold", hold, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, game, "__held?", held, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, game, "__online?", online, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, game, "loading?", loading, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, game, "loading_scene", loading_scene, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, game, "loading_error", loading_error, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, game, "__cancel_scene_load", cancel_loading, MRB_ARGS_NONE());
}
}  // namespace redgem
