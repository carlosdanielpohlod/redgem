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

constexpr const char* kKeepAliveGlobal = "$__redgem_keep_alive";

struct EvalRequest {
  const std::string* code = nullptr;
  const std::string* file_name = nullptr;
  bool compile_only = false;
};

struct CallRequest {
  mrb_value receiver;
  const char* method = nullptr;
  const std::vector<mrb_value>* args = nullptr;
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
  return mrb_funcall_argv(mrb, request->receiver, mrb_intern_cstr(mrb, request->method),
                          static_cast<mrb_int>(request->args->size()), request->args->data());
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

}  // namespace

RubyVm::RubyVm() : mrb_(mrb_open()) {
  if (mrb_ == nullptr) throw std::runtime_error("cannot create the mruby interpreter");
  mrb_gv_set(mrb_, mrb_intern_cstr(mrb_, kKeepAliveGlobal), mrb_ary_new(mrb_));
}

RubyVm::~RubyVm() { mrb_close(mrb_); }

RubyResult RubyVm::check_syntax(const std::string& code, const std::string& file_name) {
  EvalRequest request{&code, &file_name, true};
  mrb_bool failed = false;
  const int arena = mrb_gc_arena_save(mrb_);
  RubyResult result;
  const mrb_value outcome = mrb_protect_error(mrb_, run_eval, &request, &failed);
  if (failed) {
    result.ok = false;
    result.error = describe_exception(outcome);
  }
  mrb_gc_arena_restore(mrb_, arena);
  return result;
}

RubyResult RubyVm::eval(const std::string& code, const std::string& file_name) {
  EvalRequest request{&code, &file_name};
  mrb_bool failed = false;
  const int arena = mrb_gc_arena_save(mrb_);
  RubyResult result;
  result.value = mrb_protect_error(mrb_, run_eval, &request, &failed);
  if (failed) {
    result.ok = false;
    result.error = describe_exception(result.value);
    result.value = mrb_nil_value();
  }
  mrb_gc_arena_restore(mrb_, arena);
  return result;
}

RubyResult RubyVm::call(mrb_value receiver, const char* method, std::initializer_list<mrb_value> args) {
  return call(receiver, method, std::vector<mrb_value>(args));
}

RubyResult RubyVm::call(mrb_value receiver, const char* method, const std::vector<mrb_value>& args) {
  CallRequest request{receiver, method, &args};
  mrb_bool failed = false;
  RubyResult result;
  result.value = mrb_protect_error(mrb_, run_call, &request, &failed);
  if (failed) {
    result.ok = false;
    result.error = describe_exception(result.value);
    result.value = mrb_nil_value();
  }
  return result;
}

RubyResult RubyVm::call_with(mrb_value receiver, const char* method, const ArgumentsMaker& make) {
  MadeCallRequest request{receiver, method, &make};
  mrb_bool failed = false;
  RubyResult result;
  result.value = mrb_protect_error(mrb_, run_made_call, &request, &failed);
  if (failed) {
    result.ok = false;
    result.error = describe_exception(result.value);
    result.value = mrb_nil_value();
  }
  return result;
}

void RubyVm::release_kept() { mrb_gv_set(mrb_, mrb_intern_cstr(mrb_, kKeepAliveGlobal), mrb_ary_new(mrb_)); }

void RubyVm::keep_alive(mrb_value value) {
  mrb_ary_push(mrb_, mrb_gv_get(mrb_, mrb_intern_cstr(mrb_, kKeepAliveGlobal)), value);
}

std::string RubyVm::describe_exception(mrb_value exception) {
  mrb_bool failed = false;
  std::string text;
  DescribeRequest request{exception, &text};
  // (Protected: an exception's own inspect may raise.)
  mrb_protect_error(mrb_, run_describe, &request, &failed);
  if (failed || text.empty()) text = "unknown Ruby error";
  return text;
}

}  // namespace redgem
