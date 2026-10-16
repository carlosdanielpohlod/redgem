#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "editor/scripts/script_editor.hpp"

namespace redgem {

// The Ruby scripts open in the editor, one tab each (like a code editor's open files).
class ScriptWorkspace {
 public:
  explicit ScriptWorkspace(std::filesystem::path scripts_dir) : scripts_dir_(std::move(scripts_dir)) {}

  const std::filesystem::path& scripts_dir() const { return scripts_dir_; }
  // Opens a script in a new tab, or brings its tab to the front if it is already open.
  // Returns the tab, or nullptr with `error` set.
  ScriptEditor* open(const std::string& file_name, std::string& error);
  ScriptEditor* find(const std::string& file_name);
  const std::vector<std::unique_ptr<ScriptEditor>>& tabs() const { return tabs_; }
  // Closes a tab, dropping unsaved changes (the UI asks first).
  void close(const std::string& file_name);

  bool any_modified() const;
  // Saves every modified tab. False (with `error`) if a file could not be written.
  bool save_all(std::string& error);

  // Picks up changes made to open files by other programs (see ScriptEditor::sync_with_disk).
  void sync_with_disk();

  // The tab to bring to the front on the next frame (set by open()); cleared when taken.
  std::optional<std::string> take_focus_request();

 private:
  std::filesystem::path scripts_dir_;
  std::vector<std::unique_ptr<ScriptEditor>> tabs_;
  std::optional<std::string> focus_request_;
};

}  // namespace redgem
