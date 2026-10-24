#include "script/ruby_vm.hpp"

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/compile.h>
#include <mruby/error.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#include <stdexcept>

namespace redgem {
namespace {

struct EvalRequest {
  const std::string* code = nullptr;
  const std::string* file_name = nullptr;
  bool compile_only = false;
};

// The arguments are borrowed from the caller for the length of the call: no copy is made, which
// matters because every behaviour is called this way several times a frame.
struct CallRequest {
  mrb_value receiver;
  const char* method = nullptr;
  const mrb_value* argv = nullptr;
  mrb_int argc = 0;
};

mrb_value run_eval(mrb_state* mrb, void* userdata) {
  const auto* request = static_cast<const EvalRequest*>(userdata);
  mrbc_context* context = mrbc_context_new(mrb);
  mrbc_filename(mrb, context, request->file_name->c_str());
  context->no_exec = request->compile_only;
  context->capture_errors = request->compile_only;  // report instead of printing to stderr
  const mrb_value result =
      mrb_load_nstring_cxt(mrb, request->code->data(), static_cast<mrb_int>(request->code->size()), context);
  mrbc_context_free(mrb, context);
  if (mrb->exc != nullptr) mrb_exc_raise(mrb, mrb_obj_value(mrb->exc));
  return result;
}

mrb_value run_call(mrb_state* mrb, void* userdata) {
  const auto* request = static_cast<const CallRequest*>(userdata);
  return mrb_funcall_argv(mrb, request->receiver, mrb_intern_cstr(mrb, request->method), request->argc, request->argv);
}

struct MadeCallRequest {
  mrb_value receiver;
  const char* method = nullptr;
  const RubyVm::ArgumentsMaker* make = nullptr;
};

// The arguments, or nothing with `trouble` saying why. (Ruby's own errors are not caught here:
// they pass on to whoever protects the call. What C++ throws must not cross the interpreter.)
std::vector<mrb_value> made_arguments(mrb_state* mrb, const RubyVm::ArgumentsMaker& make, std::string& trouble) {
  try {
    return make(mrb);
  } catch (const std::exception& error) {
    trouble = std::string("cannot make the arguments: ") + error.what();
    return {};
  }
}

mrb_value run_made_call(mrb_state* mrb, void* userdata) {
  const auto* request = static_cast<const MadeCallRequest*>(userdata);
  std::string trouble;
  const std::vector<mrb_value> args = made_arguments(mrb, *request->make, trouble);
  if (!trouble.empty()) mrb_raise(mrb, E_RUNTIME_ERROR, trouble.c_str());
  return mrb_funcall_argv(mrb, request->receiver, mrb_intern_cstr(mrb, request->method),
                          static_cast<mrb_int>(args.size()), args.data());
}

struct DescribeRequest {
  mrb_value exception;
  std::string* out = nullptr;
};

void append_backtrace(mrb_state* mrb, mrb_value exception, std::string& out) {
  const mrb_value backtrace = mrb_funcall(mrb, exception, "backtrace", 0);
  const bool listed = mrb_array_p(backtrace);
  if (!listed) return;
  for (mrb_int i = 0; i < RARRAY_LEN(backtrace); ++i) {
    out += "\n    from ";
    out += mrb_str_to_cstr(mrb, mrb_obj_as_string(mrb, mrb_ary_entry(backtrace, i)));
  }
}

mrb_value run_describe(mrb_state* mrb, void* userdata) {
  const auto* request = static_cast<const DescribeRequest*>(userdata);
  const mrb_value message = mrb_funcall(mrb, request->exception, "inspect", 0);
  *request->out = mrb_str_to_cstr(mrb, mrb_obj_as_string(mrb, message));
  append_backtrace(mrb, request->exception, *request->out);
  return mrb_nil_value();
}

constexpr const char* kTimeoutClass = "ScriptTimeout";  // what a call that ran past its budget ends with

// Counts a call from C++ against the budget for as long as it lasts.
class BudgetedCall {
 public:
  explicit BudgetedCall(ScriptBudget& budget) : budget_(budget) { budget_.begin_call(); }
  ~BudgetedCall() { budget_.end_call(); }
  BudgetedCall(const BudgetedCall&) = delete;
  BudgetedCall& operator=(const BudgetedCall&) = delete;

 private:
  ScriptBudget& budget_;
};

// The error a call that runs too long ends with. Not a StandardError: a bare `rescue` does not
// take it for one of the script's own. (Rooted: a script removing the constant cannot free it.)
RClass* define_timeout_class(mrb_state* mrb) {
  RClass* timeout = mrb_define_class(mrb, kTimeoutClass, mrb->eException_class);
  mrb_gc_register(mrb, mrb_obj_value(timeout));
  return timeout;
}

}  // namespace

// (The allocator's data is how the code fetch hook finds the budget: mruby has no other slot free.)
RubyVm::RubyVm() : mrb_(mrb_open_allocf(mrb_default_allocf, &budget_)) {
  if (mrb_ == nullptr) throw std::runtime_error("cannot create the mruby interpreter");
  budget_.attach(mrb_, define_timeout_class(mrb_));
}

void RubyVm::set_budget(std::chrono::milliseconds budget) { budget_.set_limit(budget); }

// What went wrong, as Ruby tells it: the exception inspected, then where it came from.
std::string RubyVm::describe_exception(mrb_value exception) {
  mrb_bool failed = false;
  std::string text;
  DescribeRequest request{exception, &text};
  // (Protected, and on a budget of its own: an exception's own inspect may raise, or never return.)
  const BudgetedCall budgeted(budget_);
  mrb_protect_error(mrb_, run_describe, &request, &failed);
  const bool described = !failed && !text.empty();
  return described ? text : "unknown Ruby error";
}

// Runs `body` with a Ruby error coming back as a result, instead of leaving through C++.
RubyResult RubyVm::run_protected(mrb_protect_error_func* body, void* request) {
  mrb_bool failed = false;
  RubyResult result;
  {
    const BudgetedCall budgeted(budget_);
    result.value = mrb_protect_error(mrb_, body, request, &failed);
  }
  if (!failed) return result;
  result.ok = false;
  result.error = describe_exception(result.value);
  result.value = mrb_nil_value();
  return result;
}

RubyVm::~RubyVm() {
  budget_.set_limit(std::chrono::milliseconds(0));  // the watchdog lets go of the interpreter first
  mrb_close(mrb_);
}

RubyResult RubyVm::check_syntax(const std::string& code, const std::string& file_name) {
  EvalRequest request{&code, &file_name, true};
  const int arena = mrb_gc_arena_save(mrb_);
  RubyResult result = run_protected(run_eval, &request);
  result.value = mrb_nil_value();  // nothing ran: there is nothing to hand back
  mrb_gc_arena_restore(mrb_, arena);
  return result;
}

RubyResult RubyVm::eval(const std::string& code, const std::string& file_name) {
  EvalRequest request{&code, &file_name};
  const int arena = mrb_gc_arena_save(mrb_);
  const RubyResult result = run_protected(run_eval, &request);
  mrb_gc_arena_restore(mrb_, arena);
  return result;
}

RubyResult RubyVm::call(mrb_value receiver, const char* method, std::initializer_list<mrb_value> args) {
  CallRequest request{receiver, method, args.begin(), static_cast<mrb_int>(args.size())};
  return run_protected(run_call, &request);
}

RubyResult RubyVm::call(mrb_value receiver, const char* method, const std::vector<mrb_value>& args) {
  CallRequest request{receiver, method, args.data(), static_cast<mrb_int>(args.size())};
  return run_protected(run_call, &request);
}

RubyResult RubyVm::call_with(mrb_value receiver, const char* method, const ArgumentsMaker& make) {
  MadeCallRequest request{receiver, method, &make};
  return run_protected(run_made_call, &request);
}

}  // namespace redgem
