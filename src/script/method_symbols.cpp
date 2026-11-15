#include "script/method_symbols.hpp"

#include <utility>

namespace redgem {

mrb_sym MethodSymbols::of(mrb_state* mrb, const char* name) {
  std::string key(name);
  const auto found = known_.find(key);
  if (found != known_.end()) return found->second;
  const mrb_sym symbol = mrb_intern_cstr(mrb, name);
  known_.emplace(std::move(key), symbol);
  return symbol;
}

}  // namespace redgem
