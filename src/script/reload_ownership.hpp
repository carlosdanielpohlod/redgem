#pragma once
#include <mruby.h>
#include <map>
#include <string>

namespace redgem {
// Snapshots are rooted classes, keeping method procs and constant identities alive across GC.
class ReloadOwnership {
 public:
  explicit ReloadOwnership(mrb_state* mrb) : mrb_(mrb) {}
  void before_eval();
  void observe(const std::string& file);
  void require_owner(RClass* klass, const std::string& file);
  void refresh(RClass* klass);

 private:
  struct Owner {
    RClass* snapshot = nullptr;
    std::string file;
    std::string changed_by;
  };
  void consider(RClass* klass, const std::string& file);
  RClass* capture(RClass* klass);
  struct Observation {
    ReloadOwnership& ownership;
    const std::string& file;
    Observation(ReloadOwnership& owner, const std::string& source) : ownership(owner), file(source) {}
  };
  static int visit(mrb_state*, mrb_sym, mrb_value, void*);
  mrb_state* mrb_;
  std::map<RClass*, Owner> owners_;
};
}  // namespace redgem
