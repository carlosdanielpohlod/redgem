// HTTP natives: a game's web requests, and JSON in and out of them.

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>

#include <optional>

#include "net/http/http_client.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/message_values.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr const char* kUnavailable = "HTTP is only available while a game runs";
// How deep an answer's JSON may nest for a game to read it (each level a Ruby value inside another).
constexpr int kMaxAnswerDepth = 64;
constexpr int kCompact = -1;  // nlohmann's "no indentation"

net::HttpClient& http(mrb_state* mrb) { return required(mrb, context(mrb).http, kUnavailable); }

std::string text_of(mrb_value text) { return {RSTRING_PTR(text), static_cast<std::size_t>(RSTRING_LEN(text))}; }

// The headers as the prelude hands them: a Hash of Strings.
net::HttpHeaders headers_from(mrb_state* mrb, mrb_value hash) {
  net::HttpHeaders headers;
  const mrb_value names = mrb_hash_keys(mrb, hash);
  for (mrb_int i = 0; i < RARRAY_LEN(names); ++i) {
    const mrb_value name = mrb_ary_entry(names, i);
    headers.emplace_back(text_of(mrb_obj_as_string(mrb, name)),
                         text_of(mrb_obj_as_string(mrb, mrb_hash_get(mrb, hash, name))));
  }
  return headers;
}

// HTTP.__start(method, url, headers, body, timeout): the number its answer comes under. An
// ArgumentError, saying why, for a request that cannot go as it is.
mrb_value http_start(mrb_state* mrb, mrb_value) {
  const char* method = nullptr;
  const char* url = nullptr;
  mrb_value headers;
  mrb_value body;
  mrb_float timeout = 0.0;
  mrb_get_args(mrb, "zzHSf", &method, &url, &headers, &body, &timeout);
  timeout = at_least(mrb, timeout, 0.0, "a web request's timeout");
  net::HttpRequest request{method, url, headers_from(mrb, headers), text_of(body), static_cast<float>(timeout)};
  std::string error;
  const int id = http(mrb).start(std::move(request), error);
  if (id == 0) mrb_raisef(mrb, E_ARGUMENT_ERROR, "HTTP: %s", error.c_str());
  return mrb_int_value(mrb, id);
}

// HTTP.__json(value): what a request's json: becomes, as text.
mrb_value http_json(mrb_state* mrb, mrb_value) {
  const nlohmann::json json = json_from(mrb, single_arg(mrb), kJsonHolder);
  return text_value(mrb, json.dump(kCompact, ' ', false, nlohmann::json::error_handler_t::replace));
}

// An answer's body read as JSON, when it is JSON a game can take.
std::optional<nlohmann::json> answer_json(const std::string& text) {
  bool too_deep = false;
  const auto watch_depth = [&too_deep](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
    too_deep = too_deep || depth > kMaxAnswerDepth;
    return !too_deep;
  };
  nlohmann::json json = nlohmann::json::parse(text, watch_depth, false);
  const bool readable = !too_deep && !json.is_discarded();
  if (!readable) return std::nullopt;
  return json;
}

// HTTP.__parse_json(text): the value, or nil when the text is not JSON (or nests too deep).
mrb_value http_parse_json(mrb_state* mrb, mrb_value) {
  mrb_value text;
  mrb_get_args(mrb, "S", &text);
  const auto json = answer_json(text_of(text));
  return json ? value_from(mrb, *json) : mrb_nil_value();
}

}  // namespace

void install_http_bindings(mrb_state* mrb) {
  RClass* http_module = mrb_define_module(mrb, "HTTP");
  mrb_define_module_function(mrb, http_module, "__start", http_start, MRB_ARGS_REQ(5));
  mrb_define_module_function(mrb, http_module, "__json", http_json, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, http_module, "__parse_json", http_parse_json, MRB_ARGS_REQ(1));
}

}  // namespace redgem
