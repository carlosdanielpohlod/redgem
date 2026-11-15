#pragma once

#include <mruby.h>

#include <cstddef>
#include <string>
#include <unordered_map>

namespace redgem {

// The symbols of the methods C++ calls by name, each interned once per interpreter. mruby finds a
// name's symbol by a walk that can scan every symbol made so far, and a game makes thousands (the
// prelude alone does): done for every behaviour's update every frame, that walk was most of the
// frame. mruby never frees a symbol, so one found stays good for the interpreter's life.
class MethodSymbols {
 public:
  // The symbol of `name` (on the interpreter's thread; interning may raise, as mrb_intern does).
  mrb_sym of(mrb_state* mrb, const char* name);
  // How many names were interned so far (for the tests).
  std::size_t size() const { return known_.size(); }

 private:
  std::unordered_map<std::string, mrb_sym> known_;
};

}  // namespace redgem
