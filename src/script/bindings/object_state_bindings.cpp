#include <mruby.h>
#include <mruby/error.h>
#include <exception>
#include <stdexcept>
#include <string>
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
struct CheckpointFailure {
  bool invalid_argument = false;
  std::string message;

  mrb_value exception(mrb_state* mrb) const {
    auto* kind = invalid_argument ? E_ARGUMENT_ERROR : E_RUNTIME_ERROR;
    return mrb_exc_new(mrb, kind, message.data(), message.size());
  }
};

mrb_value capture_result(mrb_state* mrb, mrb_value self, bool& failed) {
  auto& capture = context(mrb).capture_object_state;
  const auto handle = wrapped_handle(mrb, self);
  CheckpointFailure failure;
  try {
    return value_from(mrb, capture(handle), HashKeys::Strings);
  } catch (const std::exception& error) {
    failure = {bad_checkpoint_argument(error), error.what()};
  }
  failed = true;
  return failure.exception(mrb);
}
mrb_value capture_state(mrb_state* mrb, mrb_value self) {
  auto& capture = context(mrb).capture_object_state;
  if (!capture) mrb_raise(mrb, E_RUNTIME_ERROR, "object checkpoints require a running game");
  bool failed = false;
  const mrb_value result = capture_result(mrb, self, failed);
  if (failed) mrb_exc_raise(mrb, result);
  return result;
}
mrb_value restore_error(mrb_state* mrb, mrb_value self) {
  const auto handle = wrapped_handle(mrb, self);
  const auto data = json_from(mrb, single_arg(mrb), kCheckpointHolder);
  auto& restore = context(mrb).restore_object_state;
  CheckpointFailure failure;
  try {
    restore(handle, data);  // resolves the handle again after converting user values
    return mrb_nil_value();
  } catch (const std::exception& error) {
    failure = {bad_checkpoint_argument(error), error.what()};
  }
  return failure.exception(mrb);
}
mrb_value restore_state(mrb_state* mrb, mrb_value self) {
  if (!context(mrb).restore_object_state) mrb_raise(mrb, E_RUNTIME_ERROR, "object checkpoints require a running game");
  const mrb_value error = restore_error(mrb, self);
  if (!mrb_nil_p(error)) mrb_exc_raise(mrb, error);
  return self;
}
}  // namespace
void install_object_state_bindings(mrb_state* mrb) {
  auto* object = mrb_class_get(mrb, kGameObjectClass);
  mrb_define_method(mrb, object, "capture_state", capture_state, MRB_ARGS_NONE());
  mrb_define_method(mrb, object, "restore_state", restore_state, MRB_ARGS_REQ(1));
}
}  // namespace redgem
