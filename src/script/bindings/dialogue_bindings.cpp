// Dialogue natives: reading a game's dialogue files (see ruby/engine/dialogue.rb).

#include <mruby.h>

#include <string>

#include "core/config.hpp"
#include "core/safe_path.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/message_values.hpp"
#include "script/dialogue_file.hpp"
#include "script/script_host.hpp"

namespace redgem {
namespace {

using namespace binding;

// Every problem of a file, one line each, after the file's name.
std::string problems_of(const std::string& name, const dialogue::DialogueFile& file) {
  std::string text = dialogue::shown_file(name) + ":";
  for (const std::string& error : file.errors) text += "\n  " + printable(error);
  return text;
}

// The text of a dialogue's file. Raises an ArgumentError for one that is not there.
std::string text_of_dialogue(mrb_state* mrb, const std::string& name) {
  const auto path = file_inside(script_host(mrb).game_dir(), dialogue::dialogue_file({}, name));
  const auto bytes = path ? read_file_up_to(*path, dialogue::kMaxFileBytes) : std::nullopt;
  if (!bytes) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "no dialogue %s: %s is not there (or is larger than %d bytes)", name.c_str(),
               dialogue::shown_file(name).c_str(), static_cast<int>(dialogue::kMaxFileBytes));
  }
  return {reinterpret_cast<const char*>(bytes->data()), bytes->size()};
}

// Dialogue.__read(name): the file of that name, read (its keys strings). An ArgumentError for a name
// a dialogue cannot have, a file that is not there, and one that is not well made, with what is wrong.
mrb_value dialogue_read(mrb_state* mrb, mrb_value) {
  const std::string name = name_arg(mrb);
  if (!dialogue::is_dialogue_name(name)) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "%v is no dialogue's name: one such as \"baker\" or \"village/baker\"",
               mrb_inspect(mrb, text_value(mrb, name)));
  }
  const dialogue::DialogueFile file = dialogue::read_dialogue(text_of_dialogue(mrb, name));
  if (!file.ok()) mrb_raise(mrb, E_ARGUMENT_ERROR, problems_of(name, file).c_str());
  return value_from(mrb, file.json, HashKeys::Strings);
}

}  // namespace

void install_dialogue_bindings(mrb_state* mrb) {
  RClass* dialogue_module = mrb_define_module(mrb, "Dialogue");
  mrb_define_module_function(mrb, dialogue_module, "__read", dialogue_read, MRB_ARGS_REQ(1));
}

}  // namespace redgem
