#pragma once

#include <mruby.h>

#include <cstddef>
#include <functional>
#include <map>
#include <string>

namespace redgem {

// The symbols of the names the engine uses again and again (the methods it calls, the instance
// variables its bindings read), each interned once for one interpreter. mruby finds a name's
// symbol by a walk that can scan every symbol made so far, and a game makes thousands (the prelude
// alone does): done for every behaviour's update every frame, that walk was most of the frame.
// mruby never frees a symbol, so one found stays good for the interpreter's life.
class SymbolCache {
 public:
  explicit SymbolCache(mrb_state* mrb) : mrb_(mrb) {}

  // The symbol of `name` (on the interpreter's thread; interning may raise, as mrb_intern does).
  mrb_sym of(const char* name);
  // How many names were interned so far.
  std::size_t size() const { return known_.size(); }

 private:
  mrb_state* mrb_;
  std::map<std::string, mrb_sym, std::less<>> known_;  // (found by a name without copying it)
};

}  // namespace redgem
