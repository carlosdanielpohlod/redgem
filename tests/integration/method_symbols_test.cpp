#include <doctest/doctest.h>
#include <mruby.h>

#include "script/method_symbols.hpp"

using namespace redgem;

namespace {

// An interpreter of its own, closed at the end of the test.
struct Interpreter {
  mrb_state* mrb = mrb_open();
  ~Interpreter() { mrb_close(mrb); }
};

}  // namespace

TEST_CASE("a method's symbol is the one mruby interns for its name") {
  Interpreter ruby;
  MethodSymbols symbols;
  CHECK(symbols.of(ruby.mrb, "__update") == mrb_intern_cstr(ruby.mrb, "__update"));
  CHECK(symbols.of(ruby.mrb, "on_trigger_enter") == mrb_intern_cstr(ruby.mrb, "on_trigger_enter"));
}

TEST_CASE("a name is interned once, however often it is asked for") {
  Interpreter ruby;
  MethodSymbols symbols;
  const mrb_sym first = symbols.of(ruby.mrb, "fixed_update");
  CHECK(symbols.of(ruby.mrb, "fixed_update") == first);
  CHECK(symbols.size() == 1);
  CHECK(symbols.of(ruby.mrb, "update") != first);
  CHECK(symbols.size() == 2);
}
