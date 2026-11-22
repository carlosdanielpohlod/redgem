#include "project/asset_settings_command.hpp"

#include <optional>
#include <utility>

#include "core/import_settings.hpp"
#include "core/safe_path.hpp"
#include "core/translations.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;

constexpr char kAssignmentMark = '=';
constexpr const char* kFieldIndent = "  ";

ToolResult refused(const std::string& asset, const std::string& error) {
  return failed(tr("project.assets.settings.refused", {{"asset", asset}, {"error", error}}));
}

std::string settings_text(const std::string& asset, const import_settings::Settings& settings) {
  std::string text = std::string(import_settings::kind_word(import_settings::kind_of(settings))) + " " + asset + "\n";
  for (const import_settings::Field& field : import_settings::fields(settings)) {
    text += kFieldIndent + field.name + " = " + field.value + "\n";
  }
  return text;
}

// Sets each "field=value" word; false and `error` at the first that cannot be set.
bool assign_all(import_settings::Settings& settings, const std::vector<std::string>& assignments, std::string& error) {
  for (const std::string& word : assignments) {
    const auto mark = word.find(kAssignmentMark);
    if (mark == std::string::npos || mark == 0) {
      error = tr("project.assets.settings.bad_assignment", {{"word", word}});
      return false;
    }
    if (!import_settings::set(settings, word.substr(0, mark), word.substr(mark + 1), error)) return false;
  }
  return true;
}

ToolResult changed_settings(const fs::path& file, const std::string& asset, import_settings::Settings settings,
                            const std::vector<std::string>& assignments) {
  std::string error;
  if (!assign_all(settings, assignments, error)) return refused(asset, error);
  if (!import_settings::save(file, settings, error)) return refused(asset, error);
  const std::string sidecar = asset + import_settings::kSidecarSuffix;
  return {kExitSuccess,
          settings_text(asset, settings) + tr("project.assets.settings.saved", {{"file", sidecar}}) + "\n",
          {}};
}

}  // namespace

ToolResult asset_settings(const fs::path& game_dir, const std::string& asset,
                          const std::vector<std::string>& assignments) {
  const auto file = file_inside(game_dir, asset);
  if (!file) return failed(tr("project.assets.settings.not_found", {{"asset", asset}}));
  std::string error;
  const auto settings = import_settings::read_checked(*file, error);
  if (!settings) return refused(asset, error);
  if (assignments.empty()) return {kExitSuccess, settings_text(asset, *settings), {}};
  return changed_settings(*file, asset, *settings, assignments);
}

}  // namespace redgem::tools
