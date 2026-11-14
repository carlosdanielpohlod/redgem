#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>

#include <filesystem>
#include <string>

#include "core/files.hpp"
#include "core/frame_profiler.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/script_host.hpp"
#include "script/script_profile.hpp"

namespace redgem {
namespace {
using namespace binding;

constexpr mrb_int kDefaultScriptRows = 20;  // Debug.script_profile without a count

FrameProfiler& profiler(mrb_state* mrb) {
  auto* value = context(mrb).profiler;
  if (!value) mrb_raise(mrb, E_RUNTIME_ERROR, "frame profiling requires a running game");
  return *value;
}
void put(mrb_state* mrb, mrb_value hash, std::string_view key, mrb_value value) {
  mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern(mrb, key.data(), key.size())), value);
}
void number(mrb_state* mrb, mrb_value hash, std::string_view key, double value) {
  put(mrb, hash, key, mrb_float_value(mrb, value));
}
mrb_value frame_value(mrb_state* mrb, const ProfileFrame& frame) {
  auto hash = mrb_hash_new(mrb);
  number(mrb, hash, "frame_ms", frame.frame_ms);
  number(mrb, hash, "interval_ms", frame.interval_ms);
  put(mrb, hash, "has_interval", mrb_bool_value(frame.has_interval));
  for (std::size_t i = 0; i < kProfilePhaseCount; ++i) {
    const auto name = profile_phase_name(static_cast<ProfilePhase>(i));
    number(mrb, hash, std::string(name) + "_ms", frame.phase_ms[i]);
  }
  return hash;
}
mrb_value stats(mrb_state* mrb, mrb_value) {
  const auto value = profiler(mrb).summary();
  auto hash = mrb_hash_new(mrb);
  put(mrb, hash, "samples", mrb_int_value(mrb, static_cast<mrb_int>(value.samples)));
  number(mrb, hash, "fps", value.frames_per_second);
  put(mrb, hash, "latest", frame_value(mrb, value.latest));
  put(mrb, hash, "mean", frame_value(mrb, value.mean));
  put(mrb, hash, "peak", frame_value(mrb, value.peak));
  return hash;
}
mrb_value enabled(mrb_state* mrb, mrb_value) { return mrb_bool_value(profiler(mrb).enabled()); }
mrb_value set_enabled(mrb_state* mrb, mrb_value) {
  mrb_bool value;
  mrb_get_args(mrb, "b", &value);
  profiler(mrb).set_enabled(value);
  return mrb_bool_value(value);
}
mrb_value visible(mrb_state* mrb, mrb_value) { return mrb_bool_value(context(mrb).stats_visible); }
mrb_value set_visible(mrb_state* mrb, mrb_value) {
  mrb_bool value;
  mrb_get_args(mrb, "b", &value);
  auto& timer = profiler(mrb);
  context(mrb).stats_visible = value;
  if (value) timer.set_enabled(true);
  return mrb_bool_value(value);
}
const ScriptProfile& script_profile(mrb_state* mrb) { return script_host(mrb).vm().sampler().profile(); }
mrb_value method_value(mrb_state* mrb, const ScriptMethodStat& method, std::size_t samples) {
  auto hash = mrb_hash_new(mrb);
  put(mrb, hash, "name", mrb_str_new(mrb, method.name.data(), method.name.size()));
  put(mrb, hash, "self", mrb_int_value(mrb, static_cast<mrb_int>(method.self)));
  put(mrb, hash, "total", mrb_int_value(mrb, static_cast<mrb_int>(method.total)));
  number(mrb, hash, "self_percent", share_percent(method.self, samples));
  number(mrb, hash, "total_percent", share_percent(method.total, samples));
  return hash;
}
// The methods the scripts' profiler caught most often running their own code, most first.
mrb_value script_methods(mrb_state* mrb, mrb_value) {
  mrb_int count = kDefaultScriptRows;
  mrb_get_args(mrb, "|i", &count);
  if (count < 0) mrb_raise(mrb, E_ARGUMENT_ERROR, "Debug.script_profile needs a count of zero or more");
  const ScriptProfile& profile = script_profile(mrb);
  auto rows = mrb_ary_new(mrb);
  for (const ScriptMethodStat& method : profile.top(static_cast<std::size_t>(count)))
    mrb_ary_push(mrb, rows, method_value(mrb, method, profile.samples()));
  return rows;
}
// Writes the whole profile to the game's data folder; the file's path.
mrb_value export_scripts(mrb_state* mrb, mrb_value) {
  const std::filesystem::path& path = context(mrb).script_profile_file;
  if (path.empty()) mrb_raise(mrb, E_RUNTIME_ERROR, "script profiles are exported only by a running game");
  std::string error;
  std::error_code ignored;
  std::filesystem::create_directories(path.parent_path(), ignored);
  if (!write_text_file(path, script_profile_report(script_profile(mrb)), error))
    mrb_raise(mrb, E_RUNTIME_ERROR, error.c_str());
  const std::string written = path.string();
  return mrb_str_new(mrb, written.data(), written.size());
}
mrb_value reset(mrb_state* mrb, mrb_value) {
  profiler(mrb).clear();
  script_host(mrb).vm().sampler().clear();
  return mrb_nil_value();
}
}  // namespace

void install_profile_bindings(mrb_state* mrb) {
  auto* debug = mrb_module_get(mrb, "Debug");
  mrb_define_module_function(mrb, debug, "profiling?", enabled, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, debug, "profiling=", set_enabled, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, debug, "stats_visible?", visible, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, debug, "stats_visible=", set_visible, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, debug, "frame_stats", stats, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, debug, "reset_frame_stats", reset, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, debug, "script_profile", script_methods, MRB_ARGS_OPT(1));
  mrb_define_module_function(mrb, debug, "export_script_profile", export_scripts, MRB_ARGS_NONE());
}
}  // namespace redgem
