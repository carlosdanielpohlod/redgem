// TestInput.leave_note and TestInput.note: small named texts one tested game leaves for another.

#include "testing/test_notes.hpp"

#include <mruby.h>
#include <mruby/string.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

#include "testing/test_input_bindings.hpp"

namespace redgem::testing {
namespace {

constexpr const char* kNotesFolder = "redgem-test-notes";  // tests/system_pairs/run_pair.sh empties it
constexpr std::size_t kMaxNoteName = 40;
constexpr std::size_t kMaxNoteBytes = 4096;
constexpr char kWordJoiner = '_';  // the one character a note's name may have besides letters and digits

// The file of a note. Its name is checked first: it must not lead anywhere else.
std::filesystem::path note_file(mrb_state* mrb, const char* name) {
  const std::string note = name;
  const bool plain =
      !note.empty() && note.size() <= kMaxNoteName &&
      std::all_of(note.begin(), note.end(), [](unsigned char c) { return std::isalnum(c) != 0 || c == kWordJoiner; });
  if (!plain) mrb_raise(mrb, E_ARGUMENT_ERROR, "a note's name is made of letters, digits and _");
  return notes_folder() / note;
}

mrb_value leave_note(mrb_state* mrb, mrb_value) {
  const char* name = nullptr;
  const char* text = nullptr;
  mrb_get_args(mrb, "zz", &name, &text);
  const std::filesystem::path file = note_file(mrb, name);
  std::error_code ignored;
  std::filesystem::create_directories(file.parent_path(), ignored);
  std::ofstream(file, std::ios::binary) << text;
  return mrb_nil_value();
}

// The note's text, or nil when nobody left one.
mrb_value read_note(mrb_state* mrb, mrb_value) {
  const char* name = nullptr;
  mrb_get_args(mrb, "z", &name);
  std::ifstream in(note_file(mrb, name), std::ios::binary);
  if (!in) return mrb_nil_value();
  std::stringstream text;
  text << in.rdbuf();
  const std::string read = text.str().substr(0, kMaxNoteBytes);
  return mrb_str_new(mrb, read.data(), static_cast<mrb_int>(read.size()));
}

}  // namespace

std::filesystem::path notes_folder() { return std::filesystem::temp_directory_path() / kNotesFolder; }

void install_test_notes(mrb_state* mrb, RClass* test_input_module) {
  mrb_define_module_function(mrb, test_input_module, "leave_note", leave_note, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, test_input_module, "note", read_note, MRB_ARGS_REQ(1));
}

}  // namespace redgem::testing
