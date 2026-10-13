#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <stdexcept>
#include <vector>

#include "script/ruby_vm.hpp"

using namespace redgem;

TEST_CASE("evaluates code and returns values") {
  RubyVm vm;
  const RubyResult result = vm.eval("1 + 2", "inline.rb");
  REQUIRE(result.ok);
  CHECK(mrb_integer(result.value) == 3);
}

TEST_CASE("Ruby exceptions become errors with backtraces") {
  RubyVm vm;
  const RubyResult result = vm.eval("def explode\n  raise ArgumentError, 'kaboom'\nend\nexplode", "boom.rb");
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("kaboom") != std::string::npos);
  CHECK(result.error.find("ArgumentError") != std::string::npos);
  CHECK(result.error.find("boom.rb") != std::string::npos);
  // The VM is still usable afterwards.
  CHECK(vm.eval("40 + 2", "after.rb").ok);
}

TEST_CASE("syntax errors are reported") {
  RubyVm vm;
  const RubyResult result = vm.eval("def broken(", "syntax.rb");
  CHECK_FALSE(result.ok);
}

TEST_CASE("calls methods and catches their errors") {
  RubyVm vm;
  REQUIRE(vm.eval("class Calc; def add(a, b) = a + b; def fail! = raise('no'); end", "calc.rb").ok);
  const mrb_value calc = vm.eval("Calc.new", "new.rb").value;
  vm.keep_alive(calc);
  const RubyResult sum = vm.call(calc, "add", {mrb_fixnum_value(2), mrb_fixnum_value(5)});
  REQUIRE(sum.ok);
  CHECK(mrb_integer(sum.value) == 7);
  CHECK_FALSE(vm.call(calc, "fail!").ok);
  CHECK_FALSE(vm.call(calc, "missing_method").ok);
}

TEST_CASE("arguments are made inside the protected call: failing to make them is an error like any other") {
  RubyVm vm;
  REQUIRE(vm.eval("class Calc; def add(a, b) = a + b; end", "calc.rb").ok);
  const mrb_value calc = vm.eval("Calc.new", "new.rb").value;
  vm.keep_alive(calc);
  const RubyResult sum = vm.call_with(
      calc, "add", [](mrb_state*) { return std::vector<mrb_value>{mrb_fixnum_value(2), mrb_fixnum_value(5)}; });
  REQUIRE(sum.ok);
  CHECK(mrb_integer(sum.value) == 7);
  // Ruby raising while the arguments are made, and C++ throwing there.
  const RubyResult raised = vm.call_with(calc, "add", [](mrb_state* mrb) -> std::vector<mrb_value> {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "no such value");
    return {};
  });
  CHECK_FALSE(raised.ok);
  CHECK(raised.error.find("no such value") != std::string::npos);
  const RubyResult thrown = vm.call_with(
      calc, "add", [](mrb_state*) -> std::vector<mrb_value> { throw std::runtime_error("out of something"); });
  CHECK_FALSE(thrown.ok);
  CHECK(thrown.error.find("out of something") != std::string::npos);
  CHECK(vm.eval("40 + 2", "after.rb").ok);
}

TEST_CASE("checking the syntax of code says what is wrong with it, and runs nothing") {
  RubyVm vm;
  const RubyResult fine = vm.check_syntax("$ran = true", "fine.rb");
  CHECK(fine.ok);
  CHECK(mrb_nil_p(fine.value));
  CHECK(mrb_nil_p(vm.eval("$ran", "ran.rb").value));  // it was only read

  const RubyResult broken = vm.check_syntax("def broken(", "broken.rb");
  CHECK_FALSE(broken.ok);
  CHECK_FALSE(broken.error.empty());
  CHECK(mrb_nil_p(broken.value));
  CHECK(vm.eval("1", "after.rb").ok);
}

TEST_CASE("a failed call hands back nothing but its error") {
  RubyVm vm;
  const RubyResult failed = vm.call(vm.eval("Object.new", "new.rb").value, "no_such_method");
  CHECK_FALSE(failed.ok);
  CHECK(mrb_nil_p(failed.value));
  CHECK(failed.error.find("no_such_method") != std::string::npos);
}
