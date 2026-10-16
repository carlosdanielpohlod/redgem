// Network natives: where the objects this machine moves are (NetworkSync), sent to the others in
// a format of their own (net/pose_codec.hpp). They arrive as the $sync message (Network::SYNC).

#include <mruby.h>

#include <string>

#include "core/math.hpp"
#include "net/network_session.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/network_bindings.hpp"

namespace redgem {
namespace {

// Network.__pose(key, position, rotation): true when it went (online, with a key a pose can carry).
mrb_value network_pose(mrb_state* mrb, mrb_value) {
  const char* key = nullptr;
  mrb_int key_size = 0;
  mrb_value position;
  mrb_value rotation;
  mrb_get_args(mrb, "soo", &key, &key_size, &position, &rotation);
  const Quat turned = quat_from_euler_degrees(binding::to_vec3(mrb, rotation));
  const std::string name(key, static_cast<std::size_t>(key_size));
  return mrb_bool_value(network(mrb).send_pose(name, binding::to_vec3(mrb, position), turned));
}

}  // namespace

void install_network_pose_bindings(RClass* network_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, network_module, "__pose", network_pose, MRB_ARGS_REQ(3));
}

}  // namespace redgem
