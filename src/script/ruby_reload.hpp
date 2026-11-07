#pragma once
#include <mruby.h>
#include <array>

namespace redgem {
inline constexpr std::size_t kReloadHookCount = 7;
inline constexpr std::array<const char*, kReloadHookCount> kReloadHookNames{
    "method_added", "attr_reader", "attr_writer", "attr_accessor", "private", "protected", "public"};
using ReloadHooks = std::array<mrb_func_t, kReloadHookCount>;
// A reload must not run game-defined method/attribute hooks while installing declarations.
bool inherits_class(RClass* klass, RClass* base);
ReloadHooks class_reload_hooks(mrb_state* mrb, RClass* receiver);
}  // namespace redgem
