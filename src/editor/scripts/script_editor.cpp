#include "editor/scripts/script_editor.hpp"

#include <cstdlib>

#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/files.hpp"
#include "editor/scripts/script_files.hpp"

namespace redgem {

bool ScriptEditor::open(const std::string& file_name, std::string& error) {
  const auto text = read_text_file(scripts_dir_ / file_name, error);
  if (!text) return false;
  file_name_ = file_name;
  text_ = *text;
  saved_text_ = *text;
  remember_disk_time();
  check_syntax();
  return true;
}

bool ScriptEditor::save(std::string& error) {
  if (!is_open()) return true;
  if (!write_text_file(path(), text_, error)) return false;
  saved_text_ = text_;
  remember_disk_time();
  check_syntax();
  if (syntax_error_.empty()) {
    log::info("saved " + file_name_);
  } else {
    log::error(syntax_error_);
  }
  return true;
}

void ScriptEditor::remember_disk_time() {
  std::error_code error;
  disk_time_ = std::filesystem::last_write_time(path(), error);
  changed_on_disk_ = false;
}

void ScriptEditor::revert() {
  std::string error;
  if (const auto on_disk = read_text_file(path(), error)) saved_text_ = *on_disk;
  text_ = saved_text_;
  remember_disk_time();
  check_syntax();
}

void ScriptEditor::sync_with_disk() {
  if (!is_open()) return;
  std::error_code error;
  const auto time = std::filesystem::last_write_time(path(), error);
  if (error) return;
  if (time == disk_time_) return;
  if (modified()) {
    changed_on_disk_ = true;  // the user decides: Save keeps this text, Revert takes the file's
    return;
  }
  revert();
}

void ScriptEditor::check_syntax() {
  const RubyResult result = vm_.check_syntax(text_, file_name_);
  // mruby reports "line N: message"; say which file too.
  syntax_error_ = result.ok ? "" : file_name_ + ": " + result.error;
}

void ScriptEditor::open_externally() const {
  if (!is_open()) return;
  platform::open_in_system(path());
}

}  // namespace redgem
