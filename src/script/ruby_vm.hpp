#pragma once

#include <mruby.h>
#include <mruby/error.h>

#include <chrono>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "script/script_budget.hpp"

namespace redgem {

struct RubyResult {
  bool ok = true;
  mrb_value value = mrb_nil_value();
  std::string error;  // class, message and backtrace when !ok
};

// Owns an mruby interpreter. Every entry point from C++ is protected: Ruby exceptions
// come back as RubyResult errors instead of aborting. With a budget set, a call that runs longer
// than it is ended with a ScriptTimeout error (see script/script_budget.hpp); a recursion too deep
// ends with mruby's own SystemStackError, long before the program's stack runs out.
class RubyVm {
 public:
  RubyVm();
  ~RubyVm();
  RubyVm(const RubyVm&) = delete;
  RubyVm& operator=(const RubyVm&) = delete;

  mrb_state* state() { return mrb_; }

  RubyResult eval(const std::string& code, const std::string& file_name);
  // Compiles without running. On a syntax error, `error` says where ("line N: ...").
  RubyResult check_syntax(const std::string& code, const std::string& file_name);
  RubyResult call(mrb_value receiver, const char* method, std::initializer_list<mrb_value> args = {});
  RubyResult call(mrb_value receiver, const char* method, const std::vector<mrb_value>& args);
  // Calls a method with the arguments `make` returns, made inside the same protection as the
  // call: making Ruby values can fail too (no memory left for them, say), and that is then an
  // error like any other instead of the end of the program. For arguments made out of what came
  // from outside.
  using ArgumentsMaker = std::function<std::vector<mrb_value>(mrb_state*)>;
  RubyResult call_with(mrb_value receiver, const char* method, const ArgumentsMaker& make);

  // How long each call from C++ may run Ruby; zero (the default) for no limit.
  void set_budget(std::chrono::milliseconds budget);
  std::chrono::milliseconds budget() const { return budget_.limit(); }

 private:
  RubyResult run_protected(mrb_protect_error_func* body, void* request);
  std::string describe_exception(mrb_value exception);

  // (Before the interpreter, which keeps a pointer to it from its first instruction on.)
  ScriptBudget budget_;
  mrb_state* mrb_;
};

}  // namespace redgem
