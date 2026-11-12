// Network natives: how a join checks the machine that answers it, and what a game hosted here
// proves itself with.

#include <mruby.h>

#include "core/log.hpp"
#include "net/network_session.hpp"
#include "net/secure/fingerprint.hpp"
#include "net/secure/server_pins.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/network_bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr const char* kNotAFingerprint =
    "fingerprint: is a server's whole fingerprint (64 hexadecimal digits) or a hosted game's code (10)";

// What game.json says of the server at that address and port: its fingerprint, or nothing to check.
net::TlsTrust known_trust(mrb_state* mrb, const std::string& address, std::uint16_t port) {
  const std::string digits = net::pinned_fingerprint(context(mrb).server_pins, address, port);
  if (!digits.empty()) return net::TlsTrust::pinned(digits);
  log::info("the connection to " + address + " is encrypted, but who answers is not checked: give its fingerprint");
  return net::TlsTrust::unverified();
}

// The fingerprint of the game hosted here, as people read it; nil when there is none.
mrb_value network_fingerprint(mrb_state* mrb, mrb_value) {
  const std::string digits = network(mrb).fingerprint();
  return digits.empty() ? mrb_nil_value() : text_value(mrb, net::readable_fingerprint(digits));
}

// The code a player who joins the game hosted here checks it with; nil when there is none.
mrb_value network_host_code(mrb_state* mrb, mrb_value) {
  const std::string digits = network(mrb).fingerprint();
  return digits.empty() ? mrb_nil_value() : text_value(mrb, net::host_code(digits));
}

// A fingerprint or a host's code as its digits alone (what the natives that connect take), checked
// before the game this machine is in is left: ArgumentError when it is neither.
mrb_value network_pin(mrb_state* mrb, mrb_value) {
  const char* text = nullptr;
  mrb_get_args(mrb, "z", &text);
  const std::string digits = net::pin_digits(text).value_or(std::string());
  if (digits.empty()) mrb_raise(mrb, E_ARGUMENT_ERROR, kNotAFingerprint);
  return text_value(mrb, digits);
}

}  // namespace

net::TlsTrust trust_arg(mrb_state* mrb, const char* fingerprint, const std::string& address, std::uint16_t port) {
  if (fingerprint == nullptr) return known_trust(mrb, address, port);
  const std::string digits = net::pin_digits(fingerprint).value_or(std::string());
  if (digits.empty()) mrb_raise(mrb, E_ARGUMENT_ERROR, kNotAFingerprint);
  return net::TlsTrust::pinned(digits);
}

void install_network_security_bindings(RClass* network_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, network_module, "fingerprint", network_fingerprint, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "host_code", network_host_code, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "__pin", network_pin, MRB_ARGS_REQ(1));
}

}  // namespace redgem
