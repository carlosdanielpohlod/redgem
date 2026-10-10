#pragma once

#include <filesystem>
#include <string>

#include "script/ruby_vm.hpp"

namespace redgem {

// A Ruby file open in one of the editor's script tabs.
class ScriptEditor {
 public:
  explicit ScriptEditor(std::filesystem::path scripts_dir) : scripts_dir_(std::move(scripts_dir)) {}

  bool open(const std::string& file_name, std::string& error);
  bool is_open() const { return !file_name_.empty(); }
  const std::string& file_name() const { return file_name_; }
  std::filesystem::path path() const { return scripts_dir_ / file_name_; }
  const std::filesystem::path& scripts_dir() const { return scripts_dir_; }

  std::string& text() { return text_; }
  bool modified() const { return text_ != saved_text_; }

  // Writes the file (even with mistakes, so no work is lost) and checks its syntax.
  // Returns false only when the file could not be written.
  bool save(std::string& error);
  // Back to what is on disk (which is the last save, unless another program changed the file).
  void revert();
  // Picks up changes another program made to the file: an unmodified tab loads them; a modified
  // one keeps its text and reports changed_on_disk() until it is saved or reverted.
  void sync_with_disk();
  bool changed_on_disk() const { return changed_on_disk_; }
  // Syntax problem found by the last save or open ("file.rb: line N: message"), or empty.
  const std::string& syntax_error() const { return syntax_error_; }
  // Opens the file in the system's default application for .rb files.
  void open_externally() const;

 private:
  void check_syntax();
  void remember_disk_time();

  std::filesystem::path scripts_dir_;
  std::string file_name_;
  std::string text_;
  std::string saved_text_;
  std::string syntax_error_;
  std::filesystem::file_time_type disk_time_{};  // of the file when it was last read or written here
  bool changed_on_disk_ = false;
  RubyVm vm_;
};

}  // namespace redgem
