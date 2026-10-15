// Handing the answers of a game's web requests to the blocks waiting for them.

#include <mruby.h>
#include <mruby/hash.h>

#include <map>
#include <string>
#include <vector>

#include "script/bindings/binding_helpers.hpp"
#include "script/script_host.hpp"

namespace redgem {

namespace {

constexpr const char* kHttpModule = "HTTP";
constexpr const char* kAnswerEntry = "__answer";
constexpr const char* kJoiner = ", ";  // between the values of a header the answer gave more than once

// The headers by their names, with the values of a header the answer gave more than once joined.
std::map<std::string, std::string> joined(const net::HttpHeaders& headers) {
  std::map<std::string, std::string> all;
  for (const auto& [name, value] : headers) {
    const auto [entry, first] = all.try_emplace(name, value);
    if (!first) entry->second += kJoiner + value;
  }
  return all;
}

// The headers as a Hash of Strings, by their names in lower case.
mrb_value headers_value(mrb_state* mrb, const net::HttpHeaders& headers) {
  const mrb_value hash = mrb_hash_new(mrb);
  for (const auto& [name, value] : joined(headers)) {
    mrb_hash_set(mrb, hash, binding::text_value(mrb, name), binding::text_value(mrb, value));
  }
  return hash;
}

// An answer as what HTTP.__answer takes: id, status, headers, body, error (nil when it came).
std::vector<mrb_value> answer_arguments(mrb_state* mrb, const net::HttpResult& result) {
  const net::HttpResponse& response = result.response;
  const mrb_value error = response.error.empty() ? mrb_nil_value() : binding::text_value(mrb, response.error);
  return {mrb_int_value(mrb, result.id), mrb_int_value(mrb, response.status), headers_value(mrb, response.headers),
          binding::text_value(mrb, response.body), error};
}

}  // namespace

void ScriptHost::dispatch(const net::HttpResult& result) {
  mrb_state* mrb = vm_.state();
  const int arena = mrb_gc_arena_save(mrb);
  // (The answer becomes Ruby values inside the protected call: without memory for them, one answer is lost.)
  const RubyResult called = vm_.call_with(mrb_obj_value(mrb_module_get(mrb, kHttpModule)), kAnswerEntry,
                                          [&result](mrb_state* vm) { return answer_arguments(vm, result); });
  mrb_gc_arena_restore(mrb, arena);
  if (!called.ok) report_error("an HTTP block", called.error);
}

}  // namespace redgem
