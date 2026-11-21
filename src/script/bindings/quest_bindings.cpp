// Quest natives: reading a game's quest files (see ruby/engine/quests.rb).

#include <mruby.h>
#include <mruby/array.h>

#include <string>
#include <vector>

#include "core/safe_path.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/message_values.hpp"
#include "script/quest_file.hpp"
#include "script/script_host.hpp"

namespace redgem {
namespace {

using namespace binding;

// Every problem of a file, one line each, after the file's name.
std::string problems_of(const std::string& name, const quest::QuestFile& file) {
  std::string text = quest::shown_file(name) + ":";
  for (const std::string& error : file.errors) text += "\n  " + printable(error);
  return text;
}

// The text of a quest's file. Raises an ArgumentError for one that is not there.
std::string text_of_quest(mrb_state* mrb, const std::string& name) {
  const auto path = file_inside(script_host(mrb).game_dir(), quest::quest_file({}, name));
  const auto bytes = path ? read_file_up_to(*path, quest::kMaxFileBytes) : std::nullopt;
  if (!bytes) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "no quest %s: %s is not there (or is larger than %d bytes)", name.c_str(),
               quest::shown_file(name).c_str(), static_cast<int>(quest::kMaxFileBytes));
  }
  return {reinterpret_cast<const char*>(bytes->data()), bytes->size()};
}

// Quests.__read(name): the file of that name, read (its keys strings). An ArgumentError for a name
// a quest cannot have, a file that is not there, and one that is not well made, with what is wrong.
mrb_value quest_read(mrb_state* mrb, mrb_value) {
  const std::string name = name_arg(mrb);
  if (!quest::is_quest_name(name)) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR,
               "%v is no quest's name: one such as \"bread\" or \"village/bread\", of at most %d bytes",
               mrb_inspect(mrb, text_value(mrb, name)), static_cast<int>(quest::kMaxNameBytes));
  }
  const quest::QuestFile file = quest::read_quest(text_of_quest(mrb, name));
  if (!file.ok()) mrb_raise(mrb, E_ARGUMENT_ERROR, problems_of(name, file).c_str());
  return value_from(mrb, file.json, HashKeys::Strings);
}

// Quests.__exists?(name): whether the game has a file for a quest of that name.
mrb_value quest_exists(mrb_state* mrb, mrb_value) {
  const std::string name = name_arg(mrb);
  if (!quest::is_quest_name(name)) return mrb_false_value();
  const auto path = file_inside(script_host(mrb).game_dir(), quest::quest_file({}, name));
  return mrb_bool_value(path.has_value());
}

// Quests.__names: the names of the game's quest files, sorted.
mrb_value quest_names(mrb_state* mrb, mrb_value) {
  const std::vector<std::string> names = quest::quest_names(script_host(mrb).game_dir());
  mrb_value list = mrb_ary_new_capa(mrb, static_cast<mrb_int>(names.size()));
  for (const std::string& name : names) mrb_ary_push(mrb, list, text_value(mrb, name));
  return list;
}

}  // namespace

void install_quest_bindings(mrb_state* mrb) {
  RClass* quests_module = mrb_define_module(mrb, "Quests");
  mrb_define_module_function(mrb, quests_module, "__read", quest_read, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, quests_module, "__exists?", quest_exists, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, quests_module, "__names", quest_names, MRB_ARGS_NONE());
}

}  // namespace redgem
