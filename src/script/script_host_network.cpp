// Handing what happens on the network to the scripts listening for it.

#include <mruby.h>

#include "core/safe_path.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/message_values.hpp"
#include "script/script_host.hpp"

namespace redgem {

namespace {

constexpr const char* kNetworkModule = "Network";
constexpr const char* kNetworkEventEntry = "__event";
constexpr const char* kSceneResetEntry = "__scene_reset";

// What Ruby calls each kind of network event.
struct EventName {
  net::NetworkEvent::Kind kind;
  const char* name;
};
constexpr const char* kMessageEvent = "message";
constexpr EventName kEventNames[] = {
    {net::NetworkEvent::Kind::Connected, "connected"},
    {net::NetworkEvent::Kind::Failed, "failed"},
    {net::NetworkEvent::Kind::Disconnected, "disconnected"},
    {net::NetworkEvent::Kind::PeerJoined, "joined"},
    {net::NetworkEvent::Kind::PeerLeft, "left"},
    {net::NetworkEvent::Kind::Message, kMessageEvent},
};

const char* network_event_name(net::NetworkEvent::Kind kind) {
  for (const EventName& known : kEventNames) {
    if (known.kind == kind) return known.name;
  }
  return kMessageEvent;
}

// A network event as what Network.__event takes. This makes Ruby values, which can fail: only
// for where Ruby errors are caught.
std::vector<mrb_value> event_arguments(mrb_state* mrb, const net::NetworkEvent& event) {
  return {mrb_symbol_value(mrb_intern_cstr(mrb, network_event_name(event.kind))), mrb_int_value(mrb, event.peer),
          binding::text_value(mrb, event.name), value_from(mrb, event.data)};
}

mrb_value network_module(mrb_state* mrb) { return mrb_obj_value(mrb_module_get(mrb, kNetworkModule)); }

}  // namespace

void ScriptHost::scene_was_reset() {
  const RubyResult result = vm_.call(network_module(vm_.state()), kSceneResetEntry);
  if (!result.ok) report_error("resetting the network's handlers", result.error);
}

void ScriptHost::dispatch(const net::NetworkEvent& event) {
  mrb_state* mrb = vm_.state();
  const int arena = mrb_gc_arena_save(mrb);
  // What another machine sent becomes Ruby values inside the protected call: should there be no
  // memory for them, that is one message lost, not the game.
  const RubyResult result = vm_.call_with(network_module(mrb), kNetworkEventEntry,
                                          [&event](mrb_state* vm) { return event_arguments(vm, event); });
  mrb_gc_arena_restore(mrb, arena);
  if (result.ok) return;
  // A failing handler does not take the game down, nor the connection. (The message's name is the
  // other machine's choice: it is shown with nothing in it that fakes log lines.)
  const std::string handler = std::string(network_event_name(event.kind)) + " " + printable(event.name);
  report_error("a Network handler (" + handler + ")", result.error);
}

}  // namespace redgem
