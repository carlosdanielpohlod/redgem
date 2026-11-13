#pragma once

#include <mruby.h>

#include <cstddef>
#include <string>
#include <vector>

namespace redgem {

// The constants at the top level of a game's Ruby when its scripts start: the engine's classes
// and modules (Vector3, Scene, Input...), its prelude's, and Ruby's own. A script that sets one of
// them to something else (`Vector3 = 5`) or removes it would break every engine call that uses it;
// instead the host puts it back and reports the script's mistake (see ScriptHost). mruby sets a
// constant without asking anyone, so this is a check after the fact, not a lock.
class EngineConstants {
 public:
  // Remembers every top-level constant defined now, and keeps their values alive.
  void remember(mrb_state* mrb);
  // Puts back each remembered constant a script set to something else or removed; the names put back.
  std::vector<std::string> restore(mrb_state* mrb) const;
  std::size_t size() const { return names_.size(); }

 private:
  std::vector<mrb_sym> names_;
  // The remembered values, in names_' order: a root of the collector, which ObjectSpace does not
  // list (so a script can neither empty it nor let a replaced class be freed).
  mrb_value values_ = mrb_nil_value();
};

// What a script is told when it set the engine's constants `names` to something else.
std::string reassigned_constants_error(const std::vector<std::string>& names);

}  // namespace redgem
