#include "script/symbol_cache.hpp"

namespace redgem {

mrb_sym SymbolCache::of(const char* name) {
  const auto found = known_.find(name);
  if (found != known_.end()) return found->second;
  const mrb_sym symbol = mrb_intern_cstr(mrb_, name);
  known_.emplace(name, symbol);
  return symbol;
}

}  // namespace redgem
