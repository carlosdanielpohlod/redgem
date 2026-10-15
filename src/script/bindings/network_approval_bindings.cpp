// Network natives: connection approval. What a join carries for the game joined to decide on,
// and that game's decisions.

#include <mruby.h>

#include "net/network_session.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/message_values.hpp"
#include "script/bindings/network_bindings.hpp"

namespace redgem {
namespace {

// Network.__approval(on): whether the game decides who comes in (Network.authenticate).
mrb_value network_approval(mrb_state* mrb, mrb_value) {
  network(mrb).require_approval(binding::bool_arg(mrb));
  return mrb_nil_value();
}

// Network.__decide(ticket, accepted, reason): false when there is nobody to decide on any more.
mrb_value network_decide(mrb_state* mrb, mrb_value) {
  mrb_int ticket = 0;
  mrb_bool accepted = false;
  mrb_value reason;
  mrb_get_args(mrb, "ibS", &ticket, &accepted, &reason);
  return mrb_bool_value(network(mrb).decide(static_cast<int>(ticket), accepted, binding::to_name(mrb, reason)));
}

}  // namespace

nlohmann::json auth_arg(mrb_state* mrb, mrb_value auth) {
  if (mrb_nil_p(auth)) return nullptr;
  const nlohmann::json json = json_from(mrb, auth);
  if (!net::wire::acceptable_auth(json)) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "auth: is a hash, at most %d bytes once written out",
               static_cast<int>(net::kMaxAuthBytes));
  }
  return json;
}

void install_network_approval_bindings(RClass* network_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, network_module, "__approval", network_approval, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, network_module, "__decide", network_decide, MRB_ARGS_REQ(3));
}

}  // namespace redgem
