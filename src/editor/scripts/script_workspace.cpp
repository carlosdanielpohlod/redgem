#include "editor/scripts/script_workspace.hpp"

#include <algorithm>

#include "core/safe_path.hpp"

namespace redgem {

ScriptEditor* ScriptWorkspace::open(const std::string& file_name, std::string& error) {
  ScriptEditor* tab = find(file_name);
  if (tab == nullptr) {
    auto created = std::make_unique<ScriptEditor>(scripts_dir_);
    if (!created->open(file_name, error)) return nullptr;
    tab = created.get();
    tabs_.push_back(std::move(created));
  }
  focus_request_ = tab->file_name();
  return tab;
}

namespace {

std::filesystem::path relative_source(const std::filesystem::path& folder, const std::filesystem::path& source) {
  std::error_code problem;
  const auto root = std::filesystem::weakly_canonical(folder, problem);
  if (problem) return {};
  const auto absolute = std::filesystem::absolute(source, problem);
  if (problem) return {};
  const auto parent = std::filesystem::weakly_canonical(absolute.parent_path(), problem);
  if (problem) return {};
  return (parent / absolute.filename()).lexically_relative(root);
}

bool valid_source(const std::filesystem::path& root, const std::filesystem::path& relative) {
  return relative.extension() == ".rb" && safe_asset_name(relative.generic_u8string()) &&
         file_inside(root, relative).has_value();
}
}  // namespace

std::filesystem::path project_script_source(const std::filesystem::path& root, const std::filesystem::path& source) {
  const auto relative = relative_source(root, source);
  return valid_source(root, relative) ? relative : std::filesystem::path{};
}

bool ScriptWorkspace::open_at(const std::filesystem::path& source, std::size_t line, std::string& error) {
  const auto relative = project_script_source(scripts_dir_, source);
  if (relative.empty()) {
    error = "cannot open script source " + printable(source.generic_u8string());
    return false;
  }
  ScriptEditor* tab = open(relative.generic_u8string(), error);
  if (tab == nullptr) return false;
  tab->show_line(line);
  return true;
}

ScriptEditor* ScriptWorkspace::find(const std::string& file_name) {
  const auto found = std::find_if(tabs_.begin(), tabs_.end(), [&](const std::unique_ptr<ScriptEditor>& tab) {
    if (tab->file_name() == file_name) return true;
#if defined(_WIN32) || defined(__APPLE__)
    std::error_code problem;
    return std::filesystem::equivalent(tab->path(), scripts_dir_ / file_name, problem);
#else
    return false;
#endif
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
    if (!tab->modified()) continue;
    if (!tab->save(error)) ok = false;
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
