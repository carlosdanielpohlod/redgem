#include "editor/script_workspace.hpp"

#include <algorithm>

namespace redgem {

ScriptEditor* ScriptWorkspace::open(const std::string& file_name, std::string& error) {
  ScriptEditor* tab = find(file_name);
  if (tab == nullptr) {
    auto created = std::make_unique<ScriptEditor>(scripts_dir_);
    if (!created->open(file_name, error)) return nullptr;
    tab = created.get();
    tabs_.push_back(std::move(created));
  }
  focus_request_ = file_name;
  return tab;
}

ScriptEditor* ScriptWorkspace::find(const std::string& file_name) {
  const auto found = std::find_if(tabs_.begin(), tabs_.end(), [&](const std::unique_ptr<ScriptEditor>& tab) {
    return tab->file_name() == file_name;
  });
  return found == tabs_.end() ? nullptr : found->get();
}

void ScriptWorkspace::close(const std::string& file_name) {
  tabs_.erase(std::remove_if(tabs_.begin(), tabs_.end(),
                             [&](const std::unique_ptr<ScriptEditor>& tab) { return tab->file_name() == file_name; }),
              tabs_.end());
  if (focus_request_ == file_name) focus_request_.reset();
}

bool ScriptWorkspace::any_modified() const {
  return std::any_of(tabs_.begin(), tabs_.end(),
                     [](const std::unique_ptr<ScriptEditor>& tab) { return tab->modified(); });
}

bool ScriptWorkspace::save_all(std::string& error) {
  bool ok = true;
  for (const auto& tab : tabs_) {
    if (tab->modified() && !tab->save(error)) ok = false;
  }
  return ok;
}

void ScriptWorkspace::sync_with_disk() {
  for (const auto& tab : tabs_) tab->sync_with_disk();
}

std::optional<std::string> ScriptWorkspace::take_focus_request() {
  std::optional<std::string> request = std::move(focus_request_);
  focus_request_.reset();
  return request;
}

}  // namespace redgem
