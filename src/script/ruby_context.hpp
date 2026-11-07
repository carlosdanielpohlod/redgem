#pragma once
#include <mruby/compile.h>
#include <stdexcept>

namespace redgem {
// Compiler contexts must be released even when the interpreter exits by a C++ exception.
class RubyContext {
 public:
  explicit RubyContext(mrb_state* mrb) : mrb_(mrb), value_(mrbc_context_new(mrb)) {
    if (!value_) throw std::runtime_error("cannot allocate a compiler context");
  }
  ~RubyContext() { mrbc_context_free(mrb_, value_); }
  RubyContext(const RubyContext&) = delete;
  RubyContext& operator=(const RubyContext&) = delete;
  mrbc_context* get() const { return value_; }
  mrbc_context* operator->() const { return value_; }

 private:
  mrb_state* mrb_;
  mrbc_context* value_;
};
}  // namespace redgem
