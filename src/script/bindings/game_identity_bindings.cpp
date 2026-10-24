// Game.version: the version the game's game.json gives it.

#include <mruby.h>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

// nil for a game whose game.json gives no version (or one that is not a version).
mrb_value game_version(mrb_state* mrb, mrb_value) {
  const std::string& version = context(mrb).game_version;
  return version.empty() ? mrb_nil_value() : text_value(mrb, version);
}

}  // namespace

void install_game_identity_bindings(mrb_state* mrb) {
  RClass* game_module = mrb_define_module(mrb, "Game");
  mrb_define_module_function(mrb, game_module, "version", game_version, MRB_ARGS_NONE());
}

}  // namespace redgem
