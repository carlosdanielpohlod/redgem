#pragma once

// The scripts' sampling profiler: while it is on, every kScriptSampleInterval of Ruby running, the
// interpreter's call stack is read and added to a ScriptProfile. It runs on the interpreter's own
// thread, from the code fetch hook (the function mruby calls before each instruction, see
// script_budget.hpp), which only reads the clock every kInstructionsPerClockRead instructions.
// Time spent in the engine's own functions is seen as the Ruby method that called them, once
// they return.

#include <mruby.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "script/script_profile.hpp"

namespace redgem {

inline constexpr std::chrono::microseconds kScriptSampleInterval{1000};
inline constexpr std::uint32_t kInstructionsPerClockRead = 256;
inline constexpr std::size_t kMaxSampledDepth = 64;  // the innermost frames of a deeper stack

class ScriptSampler {
 public:
  using Clock = std::function<std::chrono::nanoseconds()>;
  // `clock` stands in for the steady clock (tests); none reads the real one.
  explicit ScriptSampler(Clock clock = {});

  // Starting again forgets what was sampled before.
  void set_enabled(bool enabled);
  bool enabled() const { return enabled_; }
  // From the code fetch hook, on the interpreter's thread.
  void on_instruction(mrb_state* mrb);
  // Reads the stack now (on_instruction does when the interval has passed).
  void sample(mrb_state* mrb);
  void clear();
  const ScriptProfile& profile() const { return profile_; }

 private:
  bool clock_due();
  const std::string& name_of(mrb_state* mrb, const mrb_callinfo& frame);

  Clock clock_;
  ScriptProfile profile_;
  std::vector<std::string> stack_;
  // Method names already worked out, by the class that defines them and the method's symbol.
  std::map<std::pair<const RClass*, mrb_sym>, std::string> names_;
  std::chrono::nanoseconds next_sample_{0};
  std::uint32_t instructions_ = 0;
  bool enabled_ = false;
};

}  // namespace redgem
