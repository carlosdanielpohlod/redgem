#pragma once
#include <mruby.h>
#include <mruby/class.h>
#include <map>
#include <string>

namespace redgem {
// Native records never redefine a method: its lexical class and super binding stay untouched.
class ReloadOwnership {
 public:
  explicit ReloadOwnership(mrb_state* mrb);
  ~ReloadOwnership();
  ReloadOwnership(const ReloadOwnership&) = delete;
  ReloadOwnership& operator=(const ReloadOwnership&) = delete;
  void begin_eval(const std::string& file);
  void complete_eval();
  void end_eval(bool completed);
  void require_owner(RClass* klass, const std::string& file);
  void refresh(RClass* klass);

 private:
  using Methods = std::map<mrb_sym, mrb_method_t>;
  using Constants = std::map<mrb_sym, mrb_value>;
  struct Owner {
    mrb_sym name = 0;
    mrb_value roots = mrb_nil_value();
    Methods methods;
    Constants constants;
    std::string file;
    std::string changed_by;
  };
  struct Observation {
    Observation(ReloadOwnership& owner, RClass* parent, const std::string& source)
        : ownership(owner), base(parent), file(source) {}
    ReloadOwnership& ownership;
    RClass* base;
    const std::string& file;
  };
  void before_eval();
  void observe(const std::string& file);
  void settle_failed_eval();
  void forget_unbound();
  void consider(mrb_sym name, RClass* klass, const std::string& file);
  void capture(RClass* klass, Owner& owner);
  bool same_tables(RClass* klass, const Owner& owner) const;
  static int visit(mrb_state*, mrb_sym, mrb_value, void*);
  mrb_state* mrb_;
  std::map<RClass*, Owner> owners_;
  std::size_t depth_ = 0;
  std::string active_file_;
  std::string failed_file_;
};
}  // namespace redgem
