#pragma once
#include <mruby.h>
#include <mruby/class.h>
#include <map>
#include <string>
#include <vector>

namespace redgem {
// Native records never redefine a method: its lexical class and super binding stay untouched.
class ReloadOwnership {
 public:
  explicit ReloadOwnership(mrb_state* mrb) : mrb_(mrb) {}
  void initialize();
  void observe_failure();
  void invalidate() { reliable_ = false; }
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
  void begin_source(const std::string& file);
  void observe(const std::string& file);
  void forget_unbound();
  void consider(mrb_sym name, RClass* klass, const std::string& file);
  void capture(RClass* klass, Owner& owner);
  bool same_tables(RClass* klass, const Owner& owner) const;
  void observe_change(RClass* klass, Owner& owner, const std::string& file);
  static int visit(mrb_state*, mrb_sym, mrb_value, void*);
  mrb_state* mrb_;
  std::map<RClass*, Owner> owners_;
  std::vector<std::string> files_;
  bool reliable_ = true;
  std::string failed_file_;
};
}  // namespace redgem
