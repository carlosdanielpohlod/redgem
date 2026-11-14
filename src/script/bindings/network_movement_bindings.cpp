// Network natives: movement validation, an anti-cheat on the machine hosting the game (and on a
// dedicated server's rooms): how fast and how far the others' objects may move (see
// net/movement_validator.hpp). Network.validate_movement (network_movement.rb) wraps them.

#include <mruby.h>
#include <mruby/hash.h>

#include <string>

#include "net/movement_rules.hpp"
#include "net/network_session.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/network_bindings.hpp"
#include "script/script_context.hpp"

namespace redgem {
namespace {

using namespace binding;

// A setting a script gave, from 0 to `most`: an ArgumentError naming it otherwise.
float setting(mrb_state* mrb, mrb_float given, double most, const char* key) {
  return static_cast<float>(from_to(mrb, given, 0.0, most, key));
}

// Network.__validate_movement(max_speed, teleport_distance, tolerance, kick_after, undo): an
// ArgumentError for a number past what game.json's section takes too (net/movement_rules.hpp).
mrb_value network_validate_movement(mrb_state* mrb, mrb_value) {
  mrb_float speed = 0.0, teleport = 0.0, tolerance = 0.0;
  mrb_int kick_after = 0;
  mrb_bool undo = false;
  mrb_get_args(mrb, "fffib", &speed, &teleport, &tolerance, &kick_after, &undo);
  net::MovementRules rules;
  rules.max_speed = setting(mrb, speed, net::kMostMaxSpeed, net::kMaxSpeedKey);
  rules.teleport_distance = setting(mrb, teleport, net::kMostTeleportDistance, net::kTeleportDistanceKey);
  rules.tolerance = setting(mrb, tolerance, net::kMostMovementTolerance, net::kToleranceKey);
  const mrb_float kicks = static_cast<mrb_float>(kick_after);
  rules.kick_after = static_cast<int>(setting(mrb, kicks, net::kMostKickAfter, net::kKickAfterKey));
  rules.undo = undo;
  network(mrb).validate_movement(rules);
  return mrb_nil_value();
}

// The rules the host holds the others' moves to, as a hash of the keys Network.validate_movement takes.
mrb_value network_movement_rules(mrb_state* mrb, mrb_value) {
  const net::MovementRules& rules = network(mrb).movement_rules();
  const mrb_value hash = mrb_hash_new(mrb);
  set_entry(mrb, hash, net::kMaxSpeedKey, mrb_float_value(mrb, rules.max_speed));
  set_entry(mrb, hash, net::kTeleportDistanceKey, mrb_float_value(mrb, rules.teleport_distance));
  set_entry(mrb, hash, net::kToleranceKey, mrb_float_value(mrb, rules.tolerance));
  set_entry(mrb, hash, net::kKickAfterKey, mrb_int_value(mrb, rules.kick_after));
  set_entry(mrb, hash, net::kUndoKey, mrb_bool_value(rules.undo));
  return hash;
}

bool is_peer_id(mrb_int peer) { return peer >= 0 && peer <= net::kMaxPeers; }

// How many violations of those rules machine `peer` was caught at in this game (0 for any other number).
mrb_value network_movement_violations(mrb_state* mrb, mrb_value) {
  mrb_int peer = 0;
  mrb_get_args(mrb, "i", &peer);
  if (!is_peer_id(peer)) return mrb_int_value(mrb, 0);
  return mrb_int_value(mrb, network(mrb).movement_violations(static_cast<int>(peer)));
}

// Network.__forget_movement(key): the object that goes by `key` started or stopped here (see
// NetworkSync): when the host checks its moves, it starts where its machine next says. Nothing
// where no network runs (a script's object may start anywhere).
mrb_value network_forget_movement(mrb_state* mrb, mrb_value) {
  const char* key = nullptr;
  mrb_int key_size = 0;
  mrb_get_args(mrb, "s", &key, &key_size);
  net::NetworkSession* session = context(mrb).network;
  if (session != nullptr) session->forget_movement_of_object(std::string(key, static_cast<std::size_t>(key_size)));
  return mrb_nil_value();
}

}  // namespace

void install_network_movement_bindings(RClass* network_module, mrb_state* mrb) {
  // The share of the limits a move may go past and still be believed, unless the game says.
  mrb_define_const(mrb, network_module, "MOVEMENT_TOLERANCE", mrb_float_value(mrb, net::kDefaultMovementTolerance));
  mrb_define_module_function(mrb, network_module, "__validate_movement", network_validate_movement, MRB_ARGS_REQ(5));
  mrb_define_module_function(mrb, network_module, "movement_rules", network_movement_rules, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "movement_violations", network_movement_violations, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, network_module, "__forget_movement", network_forget_movement, MRB_ARGS_REQ(1));
}

}  // namespace redgem
