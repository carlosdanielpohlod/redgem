#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/variable.h>

#include <string>
#include <vector>

#include "script/ruby_vm.hpp"
#include "script/symbol_cache.hpp"

using namespace redgem;

namespace {

// An interpreter of its own, closed at the end of the test.
struct Interpreter {
  mrb_state* mrb = mrb_open();
  ~Interpreter() { mrb_close(mrb); }
};

constexpr int kCalls = 3;

}  // namespace

TEST_CASE("a cached name's symbol is the one mruby interns for it") {
  Interpreter ruby;
  SymbolCache symbols(ruby.mrb);
  CHECK(symbols.of("__update") == mrb_intern_cstr(ruby.mrb, "__update"));
  CHECK(symbols.of("on_trigger_enter") == mrb_intern_cstr(ruby.mrb, "on_trigger_enter"));
}

TEST_CASE("a name is interned once, however often it is asked for") {
  Interpreter ruby;
  SymbolCache symbols(ruby.mrb);
  const mrb_sym first = symbols.of("fixed_update");
  CHECK(symbols.of(std::string("fixed_update").c_str()) == first);
  CHECK(symbols.size() == 1);
  CHECK(symbols.of("update") != first);
  CHECK(symbols.size() == 2);
}

TEST_CASE("the methods a RubyVm calls are looked up in its cache, once each") {
  RubyVm vm;
  REQUIRE(vm.eval("class Counter; def tick(n) = n + 1; def tock(n) = n * 2; end; $c = Counter.new", "t.rb").ok);
  const mrb_value counter = mrb_gv_get(vm.state(), mrb_intern_cstr(vm.state(), "$c"));
  const std::size_t before = vm.symbols().size();
  for (int i = 0; i < kCalls; ++i) {
    const RubyResult ticked = vm.call(counter, "tick", {mrb_int_value(vm.state(), i)});
    REQUIRE(ticked.ok);
    CHECK(mrb_integer(ticked.value) == i + 1);
    const auto argument = [i](mrb_state* mrb) { return std::vector<mrb_value>{mrb_int_value(mrb, i)}; };
    REQUIRE(vm.call_with(counter, "tock", argument).ok);
  }
  CHECK(vm.symbols().size() == before + 2);
  CHECK(vm.symbol("tick") == mrb_intern_cstr(vm.state(), "tick"));
}
