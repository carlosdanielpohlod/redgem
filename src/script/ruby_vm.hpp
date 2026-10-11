#pragma once

#include <mruby.h>

#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

namespace redgem {

struct RubyResult {
  bool ok = true;
  mrb_value value = mrb_nil_value();
  std::string error;  // class, message and backtrace when !ok
};

// Owns an mruby interpreter. Every entry point from C++ is protected: Ruby exceptions
// come back as RubyResult errors instead of aborting.
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

  // Keeps a value alive across garbage collections until the VM dies.
  void keep_alive(mrb_value value);
  // Lets go of everything keep_alive held.
  void release_kept();

 private:
  std::string describe_exception(mrb_value exception);

  mrb_state* mrb_;
};

}  // namespace redgem
