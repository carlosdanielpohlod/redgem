#include <mruby.h>
#include <exception>
#include <stdexcept>
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/message_values.hpp"

namespace redgem {
namespace {
using namespace binding;
// A fixed checkpoint schema fits well below this bound, including its vector arrays.
constexpr std::size_t kCheckpointMembers = 16;
constexpr ValueHolder kCheckpointHolder{"an object checkpoint", "a checkpoint's", "keep", kCheckpointMembers};

bool bad_checkpoint_argument(const std::exception& error) {
  return dynamic_cast<const std::invalid_argument*>(&error) != nullptr ||
         dynamic_cast<const nlohmann::json::exception*>(&error) != nullptr;
}
[[noreturn]] void checkpoint_error(mrb_state* mrb, const std::exception& error) {
  auto* kind = bad_checkpoint_argument(error) ? E_ARGUMENT_ERROR : E_RUNTIME_ERROR;
  mrb_raise(mrb, kind, error.what());
}
mrb_value capture_state(mrb_state* mrb, mrb_value self) {
  auto& capture = context(mrb).capture_object_state;
  if (!capture) mrb_raise(mrb, E_RUNTIME_ERROR, "object checkpoints require a running game");
  try {
    return value_from(mrb, capture(wrapped_handle(mrb, self)), HashKeys::Strings);
  } catch (const std::exception& error) {
    checkpoint_error(mrb, error);
  }
}
mrb_value restore_state(mrb_state* mrb, mrb_value self) {
  const auto handle = wrapped_handle(mrb, self);
  const auto data = json_from(mrb, single_arg(mrb), kCheckpointHolder);
  auto& restore = context(mrb).restore_object_state;
  if (!restore) mrb_raise(mrb, E_RUNTIME_ERROR, "object checkpoints require a running game");
  try {
    restore(handle, data);  // resolves the handle again after converting user values
  } catch (const std::exception& error) {
    checkpoint_error(mrb, error);
  }
  return self;
}
}  // namespace
void install_object_state_bindings(mrb_state* mrb) {
  auto* object = mrb_class_get(mrb, kGameObjectClass);
  mrb_define_method(mrb, object, "capture_state", capture_state, MRB_ARGS_NONE());
  mrb_define_method(mrb, object, "restore_state", restore_state, MRB_ARGS_REQ(1));
}
}  // namespace redgem
