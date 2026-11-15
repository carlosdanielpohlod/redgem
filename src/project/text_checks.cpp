// The I18n keys of a game's files, checked against its locales (see text_checks.hpp).

#include "project/text_checks.hpp"

#include <set>

#include "core/language_names.hpp"
#include "core/safe_path.hpp"
#include "core/translations.hpp"

namespace redgem::tools {
namespace {

bool game_has_text(const GameLocales& locales, const std::string& language, const std::string& key) {
  return locales.texts.find(language_key(language), key) != nullptr;
}

bool has_text_anywhere(const GameLocales& locales, const std::string& key) {
  for (const std::string& language : locales.languages) {
    if (game_has_text(locales, language, key)) return true;
  }
  return translations::all().find(kFallbackLanguage, key) != nullptr;
}

void check_text(const GameLocales& locales, CheckReport& report, const std::string& file,
                const dialogue::NameUse& text) {
  if (!has_text_anywhere(locales, text.name)) {
    report.errors.push_back(
        tr("project.check.missing_text", {{"file", file}, {"where", text.where}, {"key", text.name}}));
    return;
  }
  for (const std::string& language : locales.languages) {
    if (game_has_text(locales, language, text.name)) continue;
    report.warnings.push_back(tr("project.check.missing_translation",
                                 {{"file", file}, {"where", text.where}, {"key", text.name}, {"language", language}}));
  }
}

}  // namespace

void check_texts(const GameLocales& locales, CheckReport& report, const std::string& file,
                 const std::vector<dialogue::NameUse>& texts) {
  std::set<std::string> seen;  // (a key used twice is said once)
  for (const dialogue::NameUse& text : texts) {
    if (seen.insert(text.name).second) check_text(locales, report, file, text);
  }
}

void add_lines(std::vector<std::string>& to, const std::string& file, const std::vector<std::string>& lines) {
  for (const std::string& line : lines) to.push_back(file + ": " + printable(line));
}

}  // namespace redgem::tools
