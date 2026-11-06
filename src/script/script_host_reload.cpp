#include "script/script_host.hpp"
#include <mruby/class.h>
#include <mbedtls/sha256.h>
#include <algorithm>
#include <stdexcept>
#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "script/script_naming.hpp"

namespace redgem {
namespace {
constexpr std::size_t kPollFiles = 8;  // at most two MiB read per automatic editor poll
constexpr const char* kBaseClass = "Behaviour";
bool inherits(RClass* klass, RClass* base) {
  for (; klass; klass = klass->super)
    if (klass == base) return true;
  return false;
}
std::optional<std::string> source_in(const std::filesystem::path& folder, const std::string& name) {
  const auto path = file_inside(folder.parent_path(), folder.filename() / std::filesystem::u8path(name));
  if (!path) return {};
  const auto bytes = read_file_up_to(*path, kReloadSourceBytes);
  if (!bytes) return {};
  return std::string(bytes->begin(), bytes->end());
}
bool observe_change(std::optional<ScriptStamp>& observed, const std::optional<ScriptStamp>& stamp, bool retry) {
  if (!retry && observed == stamp) return false;
  observed = stamp;  // one attempt per version, including a missing file
  return true;
}
std::optional<ScriptStamp> stamp_of(const std::optional<std::string>& source) {
  return source ? std::optional<ScriptStamp>(script_stamp(*source)) : std::nullopt;
}
}  // namespace
ScriptStamp script_stamp(const std::string& source) {
  ScriptStamp stamp{};
  const int result =
      mbedtls_sha256(reinterpret_cast<const unsigned char*>(source.data()), source.size(), stamp.data(), 0);
  if (result != 0) throw std::runtime_error("cannot fingerprint a script source");
  return stamp;
}
void ScriptHost::remember_source(const std::filesystem::path& path, const std::string& source) {
  if (!live_reload_) return;
  const auto name = path.lexically_relative(scripts_dir_).generic_u8string();
  if (safe_asset_name(name)) observed_sources_[name] = script_stamp(source);
}
void ScriptHost::refresh_reloaded(RClass* klass) {
  for (auto& slot : behaviours_) {
    auto* actual = mrb_obj_class(vm_.state(), slot.instance);
    slot.fixed_updates = overrides(actual, "fixed_update");
    if (inherits(actual, klass)) slot.enabled = true;
  }
}
bool ScriptHost::reload_script(const std::string& script, bool retry) {
  auto* klass = class_named(class_name_for_script(script));
  if (!klass || !inherits(klass, class_named(kBaseClass))) return false;
  const auto source = source_in(scripts_dir_, script);
  const auto stamp = stamp_of(source);
  if (!observe_change(observed_sources_[script], stamp, retry)) return false;
  if (!source) return reload_unreadable(script);
  const auto result = vm_.reload_class(klass, *source, (scripts_dir_ / script).u8string());
  if (!result.ok) return reload_failed(script, result.error);
  refresh_reloaded(klass);
  log::info("reloaded " + printable(script));
  return true;
}
bool ScriptHost::reload_unreadable(const std::string& script) {
  return reload_failed(script, "file is missing, linked, unreadable or exceeds the live reload byte limit");
}
bool ScriptHost::reload_failed(const std::string& script, const std::string& error) {
  report_error("reloading " + printable(script) + " (previous code retained)", error);
  return false;
}
std::size_t ScriptHost::reload_scripts(bool retry) {
  if (!live_reload_ || loaded_scripts_.empty()) return 0;
  const auto count = retry ? loaded_scripts_.size() : std::min(kPollFiles, loaded_scripts_.size());
  auto next = loaded_scripts_.upper_bound(reload_cursor_);
  std::size_t reloaded = 0;
  for (std::size_t i = 0; i < count; ++i) {
    if (next == loaded_scripts_.end()) next = loaded_scripts_.begin();
    const auto script = *next++;
    reload_cursor_ = script;
    if (reload_script(script, retry)) ++reloaded;
  }
  return reloaded;
}
}  // namespace redgem
