// What `redgem check` says of game.json's "menu" section, how the game's menus look (see
// game_checks.hpp). The engine's Ruby reads it as a game does (UI::Theme): what it says is wrong
// with a key or a value is an error here. Every font, picture and sound it names must be in the
// game's folders.

#include <string>
#include <utility>
#include <vector>

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>

#include "app/game_settings.hpp"
#include "core/config.hpp"
#include "core/safe_path.hpp"
#include "core/translations.hpp"
#include "project/game_checks.hpp"
#include "script/script_context.hpp"
#include "script/script_host.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;

constexpr const char* kUiModule = "UI";
constexpr const char* kThemeClass = "Theme";
constexpr const char* kCheckMethod = "__check_settings";  // [problems, [[folder, name], ...]]
constexpr mrb_int kProblems = 0;
constexpr mrb_int kNamedFiles = 1;
constexpr mrb_int kFolder = 0;
constexpr mrb_int kName = 1;
// How each folder's files may be named, after their names (as the engine looks for them).
constexpr const char* kFontExtensions[] = {"", ".ttf", ".otf"};
constexpr const char* kPictureExtensions[] = {"", ".png", ".jpg", ".jpeg", ".webp"};
constexpr const char* kSoundExtensions[] = {".wav", ".ogg", ".mp3", ".flac"};

// A file the section names: its folder of the game ("fonts") and its name.
using NamedFile = std::pair<std::string, std::string>;

// What the theme's Ruby found: the problems, and the files named.
struct MenuFindings {
  std::vector<std::string> problems;
  std::vector<NamedFile> files;
};

std::string text_at(mrb_value list, mrb_int index) {
  const mrb_value item = mrb_ary_entry(list, index);
  return mrb_string_p(item) ? std::string(RSTRING_PTR(item), RSTRING_LEN(item)) : std::string();
}

MenuFindings read_findings(mrb_value found) {
  MenuFindings findings;
  const mrb_value problems = mrb_ary_entry(found, kProblems);
  const mrb_value files = mrb_ary_entry(found, kNamedFiles);
  for (mrb_int i = 0; i < RARRAY_LEN(problems); ++i) findings.problems.push_back(text_at(problems, i));
  for (mrb_int i = 0; i < RARRAY_LEN(files); ++i) {
    const mrb_value file = mrb_ary_entry(files, i);
    findings.files.emplace_back(text_at(file, kFolder), text_at(file, kName));
  }
  return findings;
}

// Whether the theme's Ruby gave its findings (an error leaves only what it said).
bool answered(const RubyResult& found) {
  const bool is_list = mrb_array_p(found.value);
  return found.ok && is_list;
}

// The section, read by the theme's Ruby in an interpreter of its own (no game plays in it).
MenuFindings theme_findings(const fs::path& game_dir, const std::string& menu) {
  ScriptContext context;
  context.menu_settings = menu;
  ScriptHost host(context, game_dir / config::kScriptsDirName);
  mrb_state* mrb = host.vm().state();
  RClass* theme = mrb_class_get_under(mrb, mrb_module_get(mrb, kUiModule), kThemeClass);
  const RubyResult found = host.vm().call(mrb_obj_value(theme), kCheckMethod);
  if (!answered(found)) return {{found.error}, {}};
  return read_findings(found.value);
}

bool file_there(const fs::path& game_dir, const NamedFile& file) {
  const fs::path folder = game_dir / file.first;
  if (file.first == config::kFontsDirName) return find_asset_file(folder, file.second, kFontExtensions).has_value();
  if (file.first == config::kImagesDirName) {
    return find_asset_file(folder, file.second, kPictureExtensions).has_value();
  }
  return find_asset_file(folder, file.second, kSoundExtensions).has_value();
}

}  // namespace

void check_menu(const fs::path& game_dir, CheckReport& report) {
  const std::string menu = read_game_settings(game_dir).menu;
  if (menu.empty()) return;
  const MenuFindings findings = theme_findings(game_dir, menu);
  for (const std::string& problem : findings.problems) {
    report.errors.push_back(tr("project.check.menu_problem", {{"problem", problem}}));
  }
  for (const NamedFile& file : findings.files) {
    if (file_there(game_dir, file)) continue;
    report.errors.push_back(tr("project.check.menu_file", {{"folder", file.first}, {"name", file.second}}));
  }
}

}  // namespace redgem::tools
